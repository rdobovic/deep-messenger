#include <onion.h>
#include <stdint.h>
#include <string.h>
#include <sqlite3.h>
#include <db_init.h>
#include <db_contact.h>
#include <sys_memory.h>
#include <helpers.h>

// Create new empty contact object
struct db_contact * db_contact_new(void) {
    struct db_contact *cont;

    cont = safe_malloc(sizeof(struct db_contact), "Failed to allocate db contact model");
    memset(cont, 0, sizeof(struct db_contact));

    return cont;
}

// Free memory for given contact, if you want to save changes
// you must first do so using save function
void db_contact_free(struct db_contact *cont) {
    free(cont);
}

// Save given contact to database
int db_contact_save(sqlite3 *db, struct db_contact *cont) {
    const char *sql;
    sqlite3_stmt *stmt;

    const char sql_insert[] = 
        "INSERT INTO client_contacts "
        "(status, nickname, onion_address, onion_pub_key, local_sig_key_pub, "
            "local_sig_key_priv, local_enc_key_pub, local_enc_key_priv, remote_sig_key_pub, "
            "remote_enc_key_pub) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    const char sql_update[] = 
        "UPDATE client_contacts SET "
            "status = ?, nickname = ?, onion_address = ?, onion_pub_key = ?, "
            "local_sig_key_pub = ?, local_sig_key_priv = ?, local_enc_key_pub = ?, "
            "local_enc_key_priv = ?, remote_sig_key_pub = ?, remote_enc_key_pub = ? "
        "WHERE id = ?";

    sql = (cont->id > 0) ? sql_update : sql_insert;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to save database contact");

    // Extract onion key from given onion address
    onion_extract_key(cont->onion_address, cont->onion_pub_key);

    if (
        SQLITE_OK != sqlite3_bind_int(stmt, 1, cont->status) ||
        SQLITE_OK != sqlite3_bind_text(stmt, 2, cont->nickname, -1, NULL) ||
        SQLITE_OK != sqlite3_bind_text(stmt, 3, cont->onion_address, ONION_ADDRESS_LEN, NULL) ||
        SQLITE_OK != sqlite3_bind_blob(stmt, 4, cont->onion_pub_key, ONION_KEY_LEN, NULL)     ||
        SQLITE_OK != sqlite3_bind_blob(stmt, 5, cont->local_sig_key_pub, DB_CONTACT_SIG_KEY_PUB_LEN, NULL)   ||
        SQLITE_OK != sqlite3_bind_blob(stmt, 6, cont->local_sig_key_priv, DB_CONTACT_SIG_KEY_PRIV_LEN, NULL) ||
        SQLITE_OK != sqlite3_bind_blob(stmt, 7, cont->local_enc_key_pub, DB_CONTACT_ENC_KEY_PUB_LEN, NULL)   ||
        SQLITE_OK != sqlite3_bind_blob(stmt, 8, cont->local_sig_key_priv, DB_CONTACT_ENC_KEY_PRIV_LEN, NULL) ||
        SQLITE_OK != sqlite3_bind_blob(stmt, 9, cont->remote_sig_key_pub, DB_CONTACT_SIG_KEY_PUB_LEN, NULL)  ||
        SQLITE_OK != sqlite3_bind_blob(stmt, 10, cont->remote_enc_key_pub, DB_CONTACT_ENC_KEY_PUB_LEN, NULL)
    ) {
        sys_db_crash(db, "Failed to bind contact fields");
    }

    if (cont->id > 0) {
        if (sqlite3_bind_int(stmt, 11, cont->id) != SQLITE_OK)
            sys_db_crash(db, "Failed to bind contact id");
    }

    if (sqlite3_step(stmt) != SQLITE_DONE)
        sys_db_crash(db, "Failed to execute contact save query");

    cont->id = sqlite3_last_insert_rowid(db);

    sqlite3_finalize(stmt);
}

// Process next step of statement, allocate and populate contact object with fetched data
static struct db_contact * db_contact_process_row(sqlite3 *db, sqlite3_stmt *stmt) {
    int rc;
    struct db_contact *cont = NULL;

    // Check if there are no results or an error occurred
    if ((rc = sqlite3_step(stmt)) != SQLITE_ROW) {
        if (rc == SQLITE_DONE)
            return cont;

        sys_db_crash(db, "Failed to fetch database contact (step)");
    }

    cont = db_contact_new();

    // Local id
    cont->id = sqlite3_column_int(stmt, 0);
    // Contact status
    cont->status = sqlite3_column_int(stmt, 1);
    // Nickname
    memcpy(cont->nickname, sqlite3_column_text(stmt, 2),
        min(DB_CONTACT_NICK_MAX_LEN, sqlite3_column_bytes(stmt, 2)));
    // Onion address
    memcpy(cont->onion_address, sqlite3_column_text(stmt, 3),
        min(ONION_ADDRESS_LEN, sqlite3_column_bytes(stmt, 3)));
    // Onion key
    memcpy(cont->onion_pub_key, sqlite3_column_blob(stmt, 4),
        min(ONION_KEY_LEN, sqlite3_column_bytes(stmt, 4)));
    // Local signing key public
    memcpy(cont->local_sig_key_pub, sqlite3_column_blob(stmt, 5),
        min(DB_CONTACT_SIG_KEY_PUB_LEN, sqlite3_column_bytes(stmt, 5)));
    // Local signing key private
    memcpy(cont->local_sig_key_priv, sqlite3_column_blob(stmt, 6),
        min(DB_CONTACT_SIG_KEY_PRIV_LEN, sqlite3_column_bytes(stmt, 6)));
    // Local signing key public
    memcpy(cont->local_enc_key_pub, sqlite3_column_blob(stmt, 7),
        min(DB_CONTACT_ENC_KEY_PUB_LEN, sqlite3_column_bytes(stmt, 7)));
    // Local signing key private
    memcpy(cont->local_enc_key_priv, sqlite3_column_blob(stmt, 8),
        min(DB_CONTACT_ENC_KEY_PRIV_LEN, sqlite3_column_bytes(stmt, 8)));
    // Remote signing key public
    memcpy(cont->remote_sig_key_pub, sqlite3_column_blob(stmt, 9),
        min(DB_CONTACT_SIG_KEY_PUB_LEN, sqlite3_column_bytes(stmt, 9)));
    // Remote signing key public
    memcpy(cont->remote_enc_key_pub, sqlite3_column_blob(stmt, 10),
        min(DB_CONTACT_ENC_KEY_PUB_LEN, sqlite3_column_bytes(stmt, 10)));

    return cont;
}

// Get contact by their local ID
struct db_contact * db_contact_get_by_pk(sqlite3 *db, int id) {
    sqlite3_stmt *stmt;
    struct db_contact *cont;

    const char sql[] = "SELECT * FROM client_contacts WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch database contact (by pk)");

    if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind contact id, when fetching");

    cont = db_contact_process_row(db, stmt);

    sqlite3_finalize(stmt);
    return cont;
}

// Get contact by ther onion address
struct db_contact * db_contact_get_by_onion(sqlite3 *db, char *onion_address) {
    sqlite3_stmt *stmt;
    struct db_contact *cont;

    const char sql[] = "SELECT * FROM client_contacts WHERE onion_address = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch database contact (by onion)");

    if (sqlite3_bind_text(stmt, 1, onion_address, ONION_ADDRESS_LEN, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind contact onion address, when fetching");

    cont = db_contact_process_row(db, stmt);

    sqlite3_finalize(stmt);
    return cont;
}

// Get contact by ther remote signing key public
struct db_contact * db_contact_get_by_rsk_pub(sqlite3 *db, uint8_t *key) {
    sqlite3_stmt *stmt;
    struct db_contact *cont;

    const char sql[] = "SELECT * FROM client_contacts WHERE remote_sig_key_pub = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch database contact (by rsk pub)");

    if (sqlite3_bind_text(stmt, 1, key, DB_CONTACT_SIG_KEY_PUB_LEN, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind contact onion address, when fetching");

    cont = db_contact_process_row(db, stmt);

    sqlite3_finalize(stmt);
    return cont;
}