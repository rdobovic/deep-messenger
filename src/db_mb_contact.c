#include <stdint.h>
#include <sqlite3.h>
#include <helpers.h>
#include <sys_memory.h>
#include <db_init.h>
#include <db_contact.h>
#include <db_mb_account.h>
#include <db_mb_contact.h>

// Create new empty contact object
struct db_mb_contact *db_mb_contact_new(void) {
    struct db_mb_contact *cont;

    cont = safe_malloc(sizeof(struct db_mb_contact), "Failed to allocate new mailbox contact");
    memset(cont, 0, sizeof(struct db_mb_contact));

    return cont;
}

// Free given contact object, if you want to save it call
// the save function first
void db_mb_contact_free(struct db_mb_contact *cont) {
    free(cont);
}

// Save changes on given object to the database
void db_mb_contact_save(sqlite3 *db, struct db_mb_contact *cont) {
    sqlite3_stmt *stmt;
    const char *sql;

    const char sql_insert[] =
        "INSERT INTO mailbox_contacts (account_id, signing_pub_key) "
        "VALUES (?, ?)";

    const char sql_update[] =
        "UPDATE mailbox_contacts SET account_id = ?, signing_pub_key = ? "
        "WHERE id = ?";

    sql = (cont->id > 0) ? sql_update : sql_insert;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to save mailbox contact into database");

    if (
        SQLITE_OK != sqlite3_bind_int(stmt, 1, cont->account_id) ||
        SQLITE_OK != sqlite3_bind_blob(stmt, 2, cont->signing_pub_key, DB_CONTACT_SIG_KEY_PUB_LEN, NULL)
    ) {
        sys_db_crash(db, "Failed to bind mailbox contact fields");
    }

    if (cont->id > 0) {
        if (sqlite3_bind_int(stmt, 3, cont->id) != SQLITE_OK)
            sys_db_crash(db, "Failed to bind mailbox contact id");
    }

    if (sqlite3_step(stmt) != SQLITE_DONE)
        sys_db_crash(db, "Failed to save mailbox contact into database (step)");

    if (cont->id == 0)
        cont->id = sqlite3_last_insert_rowid(db);

    sqlite3_finalize(stmt);
}

// Remove given contact from the database
void db_mb_contact_delete(sqlite3 *db, struct db_mb_contact *cont) {
    sqlite3_stmt *stmt;

    const char sql[] =
        "DELETE FROM mailbox_contacts WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to delete mailbox conact");

    if (sqlite3_bind_int(stmt, 1, cont->id) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind id while deleting contacts");

    if (sqlite3_step(stmt) != SQLITE_DONE)
        sys_db_crash(db, "Failed to delete mailbox conact (step)");
    
    sqlite3_finalize(stmt);
}

// Remove all contacts from database for given account
void db_mb_contact_delete_all(sqlite3 *db, struct db_mb_account *acc) {
    sqlite3_stmt *stmt;

    const char sql[] =
        "DELETE FROM mailbox_contacts WHERE account_id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to delete all mailbox conacts for given account");

    if (sqlite3_bind_int(stmt, 1, acc->id) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind account id while deleting contacts");

    if (sqlite3_step(stmt) != SQLITE_DONE)
        sys_db_crash(db, "Failed to delete all mailbox conacts for given account (step)");
    
    sqlite3_finalize(stmt);
}

static struct db_mb_contact * db_mb_contact_process_row(sqlite3 *db, sqlite3_stmt *stmt, struct db_mb_contact *dest) {
    int rc;
    struct db_mb_contact *cont = dest;

    // Check if there are no results or an error occurred
    if ((rc = sqlite3_step(stmt)) != SQLITE_ROW) {
        if (rc == SQLITE_DONE)
            return NULL;

        sys_db_crash(db, "Failed to fetch mailbox contact (step)");
    }

    if (cont == NULL)
        cont = db_mb_contact_new();

    cont->id = sqlite3_column_int(stmt, 0);
    cont->account_id = sqlite3_column_int(stmt, 1);

    memcpy(cont->signing_pub_key, sqlite3_column_blob(stmt, 2),
        min(DB_CONTACT_SIG_KEY_PUB_LEN, sqlite3_column_bytes(stmt, 2)));

    return cont;
}

// Get mailbox contact by id
struct db_mb_contact * db_mb_contact_get_by_pk(sqlite3 *db, int id, struct db_mb_contact *dest) {
    sqlite3_stmt *stmt;
    struct db_mb_contact *cont;

    const char sql[] =
        "SELECT * FROM mailbox_contacts WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch mailbox contact (by pk)");

    if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind mailbox contact id, when fetching");

    cont = db_mb_contact_process_row(db, stmt, dest);

    sqlite3_finalize(stmt);
    return cont;
}

// Get mailbox contact by associated account and signing key
struct db_mb_contact * db_mb_contact_get_by_acc_and_key(
    sqlite3 *db, struct db_mb_account *acc, uint8_t *key, struct db_mb_contact *dest
) {
    sqlite3_stmt *stmt;
    struct db_mb_contact *cont;

    const char sql[] =
        "SELECT * FROM mailbox_contacts WHERE account_id = ? AND key = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch mailbox contact (by acc and key)");

    if (
        SQLITE_OK != sqlite3_bind_int(stmt, 1, acc->id)  ||
        SQLITE_OK != sqlite3_bind_blob(stmt, 2, key, DB_CONTACT_SIG_KEY_PUB_LEN, NULL)
    )
        sys_db_crash(db, "Failed to bind mailbox contact fields, when fetching");

    cont = db_mb_contact_process_row(db, stmt, dest);

    sqlite3_finalize(stmt);
    return cont;
}

// Pull new data from the database
void db_mb_contact_refresh(sqlite3 *db, struct db_mb_contact *cont) {
    if (cont->id == 0)
        return;

    db_mb_contact_get_by_pk(db, cont->id, cont);
}
