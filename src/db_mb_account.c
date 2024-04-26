#include <db_contact.h>
#include <db_mb_account.h>
#include <stdint.h>
#include <sqlite3.h>
#include <sys_memory.h>
#include <db_init.h>

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
        SQLITE_OK != sqlite3_bind_blob(stmt, 1, acc->mailbox_id, DB_CONTACT_MAILBOX_ID_LEN, NULL) ||
        SQLITE_OK != sqlite3_bind_blob(stmt, 2, acc->signing_pub_key, DB_CONTACT_SIG_KEY_PUB_LEN, NULL)
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