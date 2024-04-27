#include <stdint.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <helpers.h>
#include <sys_memory.h>
#include <db_init.h>
#include <db_contact.h>
#include <db_mb_account.h>
#include <constants.h>

// Create new empty account object
struct db_mb_account * db_mb_account_new(void) {
    struct db_mb_account *acc;

    acc = safe_malloc(sizeof(struct db_mb_account), "Failed to allocate mailbox account");
    memset(acc, 0, sizeof(struct db_mb_account));

    return acc;
}

// Free given account object, if you want to save it
// call the save function first
void db_mb_account_free(struct db_mb_account *acc) {
    free(acc);
}

// Save changes on given object to the database
void db_mb_account_save(sqlite3 *db, struct db_mb_account *acc) {
    const char *sql;
    sqlite3_stmt *stmt;

    const char sql_insert[] =
        "INSERT INTO mailbox_accounts (mailbox_id, signing_pub_key) "
        "VALUES (?, ?)";

    const char sql_update[] =
        "UPDATE mailbox_accounts SET "
            "mailbox_id = ?, signing_pub_key = ? "
        "WHERE id = ?";

    sql = (acc->id > 0) ? sql_update : sql_insert;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to save mailbox account into database");

    if (
        SQLITE_OK != sqlite3_bind_blob(stmt, 1, acc->mailbox_id, MAILBOX_ACCOUNT_KEY_PUB_LEN, NULL) ||
        SQLITE_OK != sqlite3_bind_blob(stmt, 2, acc->signing_pub_key, CLIENT_SIG_KEY_PUB_LEN, NULL)
    ) {
        sys_db_crash(db, "Failed to bind mailbox account fields");
    }

    if (acc->id > 0) {
        if (sqlite3_bind_int(stmt, 3, acc->id) != SQLITE_OK)
            sys_db_crash(db, "Failed to bind mailbox account id");
    }

    if (sqlite3_step(stmt) != SQLITE_DONE)
        sys_db_crash(db, "Failed to save mailbox account into database (step)");

    if (acc->id == 0)
        acc->id = sqlite3_last_insert_rowid(db);

    sqlite3_finalize(stmt);
}

// Process next step of the statement and allocate or populate given object with row data
static struct db_mb_account * db_mb_account_process_row(sqlite3 *db, sqlite3_stmt *stmt, struct db_mb_account *dest) {
    int rc;
    struct db_mb_account *acc = dest;

    // Check if there are no results or an error occurred
    if ((rc = sqlite3_step(stmt)) != SQLITE_ROW) {
        if (rc == SQLITE_DONE)
            return NULL;

        sys_db_crash(db, "Failed to fetch mailbox account (step)");
    }

    if (acc == NULL)
        acc = db_mb_account_new();

    // Local id
    acc->id = sqlite3_column_int(stmt, 0);
    // Global mailbox id
    memcpy(acc->mailbox_id, sqlite3_column_blob(stmt, 1),
        min(MAILBOX_ACCOUNT_KEY_PUB_LEN, sqlite3_column_bytes(stmt, 1)));
    // Public signature key
    memcpy(acc->signing_pub_key, sqlite3_column_blob(stmt, 2),
        min(CLIENT_SIG_KEY_PUB_LEN, sqlite3_column_bytes(stmt, 2)));

    return acc;
}

// Get mailbox account by id
struct db_mb_account * db_mb_account_get_by_pk(sqlite3 *db, int id, struct db_mb_account *dest) {
    sqlite3_stmt *stmt;
    struct db_mb_account *acc;

    const char sql[] = "SELECT * FROM mailbox_accounts WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch mailbox account from database (by pk)");

    if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind mailbox account id, when fetching");

    acc = db_mb_account_process_row(db, stmt, dest);

    sqlite3_finalize(stmt);
    return acc;
}

// Get mailbox account by unique mailbox id
struct db_mb_account * db_mb_account_get_by_mbid(sqlite3 *db, uint8_t *mbid, struct db_mb_account *dest) {
    sqlite3_stmt *stmt;
    struct db_mb_account *acc;

    const char sql[] = "SELECT * FROM mailbox_accounts WHERE mailbox_id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch mailbox account from database (by mailbox id)");

    if (sqlite3_bind_blob(stmt, 1, mbid, MAILBOX_ACCOUNT_KEY_PUB_LEN, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind mailbox account mailbox id, when fetching");

    acc = db_mb_account_process_row(db, stmt, dest);

    sqlite3_finalize(stmt);
    return acc;
}

// Pull new data from the database
void db_mb_account_refresh(sqlite3 *db, struct db_mb_account *acc) {
    if (acc->id == 0)
        return;

    db_mb_account_get_by_pk(db, acc->id, acc);
}

// Delete given contact
void db_mb_account_delete(sqlite3 *db, struct db_mb_account *acc) {
    sqlite3_stmt *stmt;

    const char sql[] = "DELETE FROM mailbox_accounts WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to delete mailbox account");

    if (sqlite3_bind_int(stmt, 1, acc->id) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind mailbox account id, when deleting");

    if (sqlite3_step(stmt) != SQLITE_DONE)
        sys_db_crash(db, "Failed to delete mailbox account (step)");

    sqlite3_finalize(stmt);
}