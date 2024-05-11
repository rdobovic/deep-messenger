#include <onion.h>
#include <db_contact.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <db_message.h>
#include <sys_memory.h>
#include <db_init.h>
#include <helpers.h>
#include <sqlite3.h>
#include <constants.h>
#include <openssl/rand.h>

// Create new empty message object
struct db_message * db_message_new(void) {
    struct db_message *msg;

    msg = safe_malloc(sizeof(struct db_message), "Failed to allocate memory for message model");
    memset(msg, 0, sizeof(struct db_message));

    return msg;
}

// Free given message object, note that if you want to save changes you
// made to the object you must first save it
void db_message_free(struct db_message *msg) {
    if (msg && msg->body_text)
        free(msg->body_text);
    free(msg);
}

// Save all message data into database
void db_message_save(sqlite3 *db, struct db_message *msg) {
    const char *sql;
    sqlite3_stmt *stmt;

    const char sql_insert[] =
        "INSERT INTO client_messages "
        "(global_id, contact_id, sender, status, type, body_text, body_nick, "
            "body_mbox_id, body_mbox_onion) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";

    const char sql_update[] = 
        "UPDATE client_messages SET "
            "global_id = ?, contact_id = ?, sender = ?, status = ?, type = ?, "
            "body_text = ?, body_nick = ?, body_mbox_id = ?, body_mbox_onion = ? "
        "WHERE id = ?";

    sql = (msg->id > 0) ? sql_update : sql_insert;

    // RECV is virtual type, it should never be saved to the database
    if (msg->type == DB_MESSAGE_RECV)
        return;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to save message into database");

    if (
        SQLITE_OK != sqlite3_bind_blob(stmt, 1, msg->global_id, MESSAGE_ID_LEN, NULL) ||
        SQLITE_OK != sqlite3_bind_int(stmt, 2, msg->contact_id) ||
        SQLITE_OK != sqlite3_bind_int(stmt, 3, msg->sender)   ||
        SQLITE_OK != sqlite3_bind_int(stmt, 4, msg->status)   ||
        SQLITE_OK != sqlite3_bind_int(stmt, 5, msg->type)
    ) {
        sys_db_crash(db, "Failed to bind required message fields");
    }

    if (
        ((msg->type == DB_MESSAGE_TEXT) ?
            sqlite3_bind_text(stmt, 6, msg->body_text, -1, NULL) :
            sqlite3_bind_null(stmt, 6))
        != SQLITE_OK ||

        ((msg->type == DB_MESSAGE_NICK) ?
            sqlite3_bind_text(stmt, 7, msg->body_nick, -1, NULL) :
            sqlite3_bind_null(stmt, 7)) 
        != SQLITE_OK ||

        ((msg->type == DB_MESSAGE_MBOX) ?
            sqlite3_bind_blob(stmt, 8, msg->body_mbox_id, MAILBOX_ID_LEN, NULL) :
            sqlite3_bind_null(stmt, 8)) 
        != SQLITE_OK ||

        ((msg->type == DB_MESSAGE_MBOX) ? 
            sqlite3_bind_text(stmt, 9, msg->body_mbox_onion, -1, NULL) :
            sqlite3_bind_null(stmt, 9))
        != SQLITE_OK
    ) {
        sys_db_crash(db, "Failed to bind message body");
    }

    if (msg->id > 0) {
        if (sqlite3_bind_int(stmt, 10, msg->id) != SQLITE_OK)
            sys_db_crash(db, "Failed to bind message id");
    }

    if (sqlite3_step(stmt) != SQLITE_DONE)
        sys_db_crash(db, "Failed to save message into database (step)");

    if (msg->id == 0)
        msg->id = sqlite3_last_insert_rowid(db);

    sqlite3_finalize(stmt);
}

// Write text_len characters of text into message text body
void db_message_set_text(struct db_message *msg, const char *text, int text_len) {
    int i;
    int new_arr_len;

    if (text_len == -1)
        text_len = strlen(text);

    new_arr_len = (text_len / DB_MESSAGE_TEXT_CHUNK + 1) * DB_MESSAGE_TEXT_CHUNK;
    msg->body_text_len = text_len;

    if (!msg->body_text) {
        msg->body_text_n_chunks = new_arr_len;
        msg->body_text = safe_malloc((sizeof(char) * new_arr_len),
            "Failed to allocate chars for message body");

    } else if (msg->body_text_n_chunks < new_arr_len) {
        msg->body_text_n_chunks = new_arr_len;
        msg->body_text = safe_realloc(msg->body_text, (sizeof(char) * new_arr_len),
            "Failed to reallocate chars for message body");
    }

    for (i = 0; i < text_len; i++) {
        msg->body_text[i] = text[i];
    }

    msg->body_text[i] = '\0';
}

// Process the next step of given statement and allocate or populate given object with the row data
static struct db_message * db_message_process_row(sqlite3 *db, sqlite3_stmt *stmt, struct db_message *dest) {
    int rc;
    struct db_message *msg = dest;

    // Check if there are no results or an error occurred
    if ((rc = sqlite3_step(stmt)) != SQLITE_ROW) {
        if (rc == SQLITE_DONE)
            return NULL;

        sys_db_crash(db, "Failed to fetch message from database (step)");
    }

    if (msg == NULL)
        msg = db_message_new();

    msg->id = sqlite3_column_int(stmt, 0);

    memcpy(msg->global_id, sqlite3_column_text(stmt, 1),
        min(MESSAGE_ID_LEN, sqlite3_column_bytes(stmt, 1)));

    msg->contact_id = sqlite3_column_int(stmt, 2);
    msg->sender = sqlite3_column_int(stmt, 3);
    msg->status = sqlite3_column_int(stmt, 4);
    msg->type = sqlite3_column_int(stmt, 5);

    if (msg->type == DB_MESSAGE_TEXT) {
        db_message_set_text(msg, sqlite3_column_text(stmt, 6), sqlite3_column_bytes(stmt, 6));
    }

    if (msg->type == DB_MESSAGE_NICK) {
        msg->body_nick_len =
            min(CLIENT_NICK_MAX_LEN, sqlite3_column_bytes(stmt, 7));
        memcpy(msg->body_nick, sqlite3_column_text(stmt, 7), msg->body_nick_len);
    }

    if (msg->type == DB_MESSAGE_MBOX) {
        memcpy(msg->body_mbox_id, sqlite3_column_text(stmt, 8),
            min(MAILBOX_ID_LEN, sqlite3_column_bytes(stmt, 8)));
        memcpy(msg->body_mbox_onion, sqlite3_column_text(stmt, 9),
            min(MAILBOX_ID_LEN, sqlite3_column_bytes(stmt, 9)));
    }

    return msg;
}

// Get message by it's local ID
struct db_message * db_message_get_by_pk(sqlite3 *db, int id, struct db_message *dest) {
    sqlite3_stmt *stmt;
    struct db_message *msg;

    const char sql[] = "SELECT * FROM client_messages WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch message from database (by pk)");

    if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind message id, when fetching");

    msg = db_message_process_row(db, stmt, dest);

    sqlite3_finalize(stmt);
    return msg;
}

// Get message by it's local ID
struct db_message * db_message_get_by_gid(sqlite3 *db, uint8_t *gid, struct db_message *dest) {
    sqlite3_stmt *stmt;
    struct db_message *msg;

    const char sql[] = "SELECT * FROM client_messages WHERE global_id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch message from database (by gid)");

    if (sqlite3_bind_blob(stmt, 1, gid, MESSAGE_ID_LEN, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind message id, when fetching");

    msg = db_message_process_row(db, stmt, dest);

    sqlite3_finalize(stmt);
    return msg;
}

// Get last message for given contact
struct db_message * db_message_get_last(sqlite3 *db, struct db_contact *cont, struct db_message *dest) {
    sqlite3_stmt *stmt;
    struct db_message *msg;

    const char sql[] = 
        "SELECT * FROM client_messages WHERE contact_id = ? ORDER BY id DESC LIMIT 1";

    if (!cont)
        return NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch message from database (by contact)");

    if (sqlite3_bind_int(stmt, 1, cont->id) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind message id, when fetching");

    msg = db_message_process_row(db, stmt, dest);

    sqlite3_finalize(stmt);
    return msg;
}

struct db_message * db_message_get_before(sqlite3 *db, struct db_message *current_msg, struct db_message *dest) {
    sqlite3_stmt *stmt;
    struct db_message *msg;

    const char sql[] = 
        "SELECT * FROM client_messages "
        "WHERE id < ? AND contact_id = ? ORDER BY id DESC LIMIT 1";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch message from database (one before)");

    if (
        sqlite3_bind_int(stmt, 2, current_msg->id) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 1, current_msg->contact_id) != SQLITE_OK
    )
        sys_db_crash(db, "Failed to bind message fields, when fetching (one before)");

    msg = db_message_process_row(db, stmt, dest);

    sqlite3_finalize(stmt);
    return msg;
}

// Pull new data from the database
void db_message_refresh(sqlite3 *db, struct db_message *msg) {
    if (msg->id == 0)
        return;

    db_message_get_by_pk(db, msg->id, msg);
}

// Delete given message
void db_message_delete(sqlite3 *db, struct db_message *msg) {
    sqlite3_stmt *stmt;

    const char sql[] = "DELETE FROM client_messages WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to delete message from database");

    if (sqlite3_bind_int(stmt, 1, msg->id) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind message id, when deleting");

    if (sqlite3_step(stmt) != SQLITE_DONE)
        sys_db_crash(db, "Failed to delete message from database (step)");

    sqlite3_finalize(stmt);
}

// Generate random global message ID
void db_message_gen_id(struct db_message *msg) {
    if (msg)
        RAND_bytes(msg->global_id, MESSAGE_ID_LEN);
}

// Fetch the list of messages for given contact with given status
struct db_message ** db_message_get_all(sqlite3 *db, struct db_contact *cont, enum db_message_status status, int *n_msgs) {
    int i;
    sqlite3_stmt *stmt;
    struct db_message **msgs;

    const char sql_any[] =
        "SELECT * FROM client_messages WHERE contact_id = ? AND (status = ? OR 1=1)";
    const char sql_count_any[] =
        "SELECT COUNT(*) FROM client_messages WHERE contact_id = ? AND (status = ? OR 1=1)";
    const char sql[] =
        "SELECT * FROM client_messages WHERE contact_id = ? AND status = ?";
    const char sql_count[] =
        "SELECT COUNT(*) FROM client_messages WHERE contact_id = ? AND status = ?";

    if (sqlite3_prepare_v2(db, status == DB_MESSAGE_STATUS_ANY ? sql_count_any : sql_count, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to count client messages");

    if (
        SQLITE_OK != sqlite3_bind_int(stmt, 1, cont->id) ||
        SQLITE_OK != sqlite3_bind_int(stmt, 2, status)
    ) {
        sys_db_crash(db, "Failed to bind fields when counting client messages");
    }

    if (sqlite3_step(stmt) != SQLITE_ROW)
        sys_db_crash(db, "Failed to count client messages (step)");

    *n_msgs = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (*n_msgs == 0) return NULL;

    if (sqlite3_prepare_v2(db, status == DB_MESSAGE_STATUS_ANY ? sql_any : sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch client messages");

    if (
        SQLITE_OK != sqlite3_bind_int(stmt, 1, cont->id) ||
        SQLITE_OK != sqlite3_bind_int(stmt, 2, status)
    ) {
        sys_db_crash(db, "Failed to bind fields when fetching client messages");
    }

    msgs = safe_malloc((sizeof(struct db_message *) * (*n_msgs)),
        "Failed to allocate memory for client message list");

    for (i = 0; i < *n_msgs; i++) {
        msgs[i] = db_message_process_row(db, stmt, NULL);
    }

    sqlite3_finalize(stmt);
    return msgs;
}

// Free previously fetched message list
void db_message_free_all(struct db_message **msgs, int n_msgs) {
    int i;

    for (i = 0; i < n_msgs; i++) {
        db_message_free(msgs[i]);
    }
}