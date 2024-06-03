#include <stdint.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <helpers.h>
#include <sys_memory.h>
#include <db_init.h>
#include <db_message.h>
#include <db_mb_account.h>
#include <db_mb_message.h>
#include <constants.h>
#include <debug.h>

// Create new empty mailbox message object
struct db_mb_message * db_mb_message_new(void) {
    struct db_mb_message *msg;

    msg = safe_malloc(sizeof(struct db_mb_message), "Failed to allocate mailbox message");
    memset(msg, 0, sizeof(struct db_mb_message));

    return msg;
}

// Free given mailbox message object, if you want to save it
// call the save function first
void db_mb_message_free(struct db_mb_message *msg) {
    if (msg && msg->data)
        free(msg->data);

    free(msg);
}

// Set message content
void db_mb_message_set_data(struct db_mb_message *msg, const uint8_t *data, int data_len) {
    int i;
    int new_len;

    new_len = (data_len / DB_MB_MESSAGE_CHUNK_SIZE + 1) * DB_MB_MESSAGE_CHUNK_SIZE;
    msg->data_len = data_len;

    if (!msg->data) {
        msg->data_n_chunks = new_len;
        msg->data = safe_malloc((sizeof(uint8_t) * new_len),
            "Failed to allocate mailbox message data");

    } else if (msg->data_len < new_len) {
        msg->data_n_chunks = new_len;
        msg->data = safe_realloc(msg->data, (sizeof(uint8_t) * new_len), 
            "Failed to realloc mailbox message data");
    }

    for (i = 0; i < data_len; i++) {
        msg->data[i] = data[i];
    }
}

// Save changes on given object to database
void db_mb_message_save(sqlite3 *db, struct db_mb_message *msg) {
    sqlite3_stmt *stmt;
    const char *sql;

    const char sql_insert[] = 
        "INSERT INTO mailbox_messages (account_id, contact_id, global_id, data) "
        "VALUES (?, ?, ?, ?)";

    const char sql_update[] =
        "UPDATE mailbox_messages SET account_id = ?, contact_id = ?, global_id = ?, data = ? "
        "WHERE id = ?";

    sql = (msg->id > 0) ? sql_update : sql_insert;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to save mailbox message");

    if (
        SQLITE_OK != sqlite3_bind_int(stmt, 1, msg->account_id) ||
        SQLITE_OK != sqlite3_bind_int(stmt, 2, msg->contact_id) ||
        SQLITE_OK != sqlite3_bind_blob(stmt, 3, msg->global_id, MESSAGE_ID_LEN, NULL) ||
        SQLITE_OK != sqlite3_bind_blob(stmt, 4, msg->data, msg->data_len, NULL)
    ) {
        sys_db_crash(db, "Failed to bind mailbox message fields");
    }

    if (msg->id > 0) {
        if (sqlite3_bind_int(stmt, 4, msg->id) != SQLITE_OK)
            sys_db_crash(db, "Failed to bind mailbox message id");
    }

    if (sqlite3_step(stmt) != SQLITE_DONE)
        sys_db_crash(db, "Failed to save mailbox message (step)");

    if (msg->id == 0)
        msg->id = sqlite3_last_insert_rowid(db);

    sqlite3_finalize(stmt);
}

// Delete given message from the database
void db_mb_message_delete(sqlite3 *db, struct db_mb_message *msg) {
    sqlite3_stmt *stmt;

    const char sql[] = 
        "DELETE FROM mailbox_messages WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to delete mailbox massage form db");

    if (sqlite3_bind_int(stmt, 1, msg->id) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind mailbox message id, while deleting");

    if (sqlite3_step(stmt) != SQLITE_DONE)
        sys_db_crash(db, "Failed to delete mailbox massage form db (step)");

    sqlite3_finalize(stmt);
}

// Process next step for given statement and allocate or populate given object with row data
static struct db_mb_message * db_mb_message_process_row(
    sqlite3 *db, sqlite3_stmt *stmt, struct db_mb_message *dest
) {
    int rc;
    struct db_mb_message *msg = dest;

    // Check if there are no results or an error occurred
    if ((rc = sqlite3_step(stmt)) != SQLITE_ROW) {
        if (rc == SQLITE_DONE)
            return NULL;

        sys_db_crash(db, "Failed to fetch mailbox message from database (step)");
    }

    if (msg == NULL)
        msg = db_mb_message_new();

    msg->id = sqlite3_column_int(stmt, 0);
    msg->account_id = sqlite3_column_int(stmt, 1);
    msg->contact_id = sqlite3_column_int(stmt, 2);

    memcpy(msg->global_id, sqlite3_column_blob(stmt, 3),
        min(MESSAGE_ID_LEN, sqlite3_column_bytes(stmt, 3)));

    db_mb_message_set_data(msg, sqlite3_column_blob(stmt, 4), sqlite3_column_bytes(stmt, 4));
    return msg;
}

// Get mailbox message by id
struct db_mb_message * db_mb_message_get_by_pk(sqlite3 *db, int id, struct db_mb_message *dest) {
    sqlite3_stmt *stmt;
    struct db_mb_message *msg;

    const char sql[] = "SELECT * FROM mailbox_messages WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch mailbox message from db (by pk)");
    
    if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind mailbox message id, while fetching");

    msg = db_mb_message_process_row(db, stmt, dest);

    sqlite3_finalize(stmt);
    return msg;
}

// Get mailbox message by associated account and global message id
struct db_mb_message * db_mb_message_get_by_acc_and_gid(
    sqlite3 *db, struct db_mb_account *acc, uint8_t *gid, struct db_mb_message *dest
) {
    sqlite3_stmt *stmt;
    struct db_mb_message *msg;

    const char sql[] = 
        "SELECT * FROM mailbox_messages WHERE account_id = ? AND global_id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch mailbox message from db (by acc and gid)");
    
    if (
        SQLITE_OK != sqlite3_bind_int(stmt, 1, acc->id) ||
        SQLITE_OK != sqlite3_bind_blob(stmt, 2, gid, MESSAGE_ID_LEN, NULL)
    )
        sys_db_crash(db, "Failed to bind mailbox message fields, while fetching");

    msg = db_mb_message_process_row(db, stmt, dest);

    sqlite3_finalize(stmt);
    return msg;
}

// Get all mailbox messages for given account
struct db_mb_message ** db_mb_message_get_all(sqlite3 *db, struct db_mb_account *acc, int *n) {
    int i;
    sqlite3_stmt *stmt;
    struct db_mb_message **msgs;

    debug("Get all start");

    const char sql[] = 
        "SELECT * FROM mailbox_messages WHERE account_id = ?";
    const char sql_count[] =
        "SELECT COUNT(*) FROM mailbox_messages WHERE account_id = ?";

    if (sqlite3_prepare_v2(db, sql_count, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to count mailbox messages");

    if (sqlite3_bind_int(stmt, 1, acc->id) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind account id when counting mb messages");

    if (sqlite3_step(stmt) != SQLITE_ROW)
        sys_db_crash(db, "Failed to count mailbox messages (step)");

    *n = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    debug("Get all got cnt");

    if (*n == 0) return NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to mailbox messages");

    if (sqlite3_bind_int(stmt, 1, acc->id) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind account id when fetching mb messages");

    msgs = safe_malloc((sizeof(struct db_mb_message *) * (*n)),
        "Failed to allocate memory for mailbox message list");

    debug("Before process row");

    for (i = 0; i < *n; i++) {
        msgs[i] = db_mb_message_process_row(db, stmt, NULL);
    }

    debug("After process row");

    sqlite3_finalize(stmt);
    debug("Get all end");
    return msgs;
}

// Free list of messages returned by get_all
void db_mb_message_free_all(struct db_mb_message **msgs, int n) {
    int i;

    for (i = 0; i < n; i++) {
        db_mb_message_free(msgs[i]);
    }
    free(msgs);
}