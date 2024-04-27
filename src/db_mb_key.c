#include <db_init.h>
#include <db_mb_key.h>
#include <sqlite3.h>
#include <stdint.h>
#include <helpers.h>
#include <sys_memory.h>
#include <constants.h>

// Create new empty key object
struct db_mb_key * db_mb_key_new(void) {
    struct db_mb_key *key;

    key = safe_malloc(sizeof(struct db_mb_key), "Failed to allocate mailbox key object");
    memset(key, 0, sizeof(struct db_mb_key));

    return key;
}

// Free given key object, if you want to save it call the
// save function first
void db_mb_key_free(struct db_mb_key *key) {
    free(key);
}

// Save changes on given object to database
void db_mb_key_save(sqlite3 *db, struct db_mb_key *key) {
    sqlite3_stmt *stmt;
    const char *sql;

    const char sql_insert[] =
        "INSERT INTO mailbox_keys (key, uses_left)"
        "VALUES (?, ?)";
    
    const char sql_update[] =
        "UPDATE mailbox_keys SET key = ?, uses_left = ? "
        "WHERE id = ?";

    sql = (key->id > 0) ? sql_update : sql_insert;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to save mailbox key into the database");

    if (
        SQLITE_OK != sqlite3_bind_blob(stmt, 1, key->key, MAILBOX_ACCESS_KEY_LEN, NULL) ||
        SQLITE_OK != sqlite3_bind_int(stmt, 2, key->uses_left)
    ) {
        sys_db_crash(db, "Failed to bind mailbox key fields");
    }

    if (key->id > 0) {
        if (sqlite3_bind_int(stmt, 3, key->id) != SQLITE_OK)
            sys_db_crash(db, "Failed to bind mailbox key id");
    }

    if (sqlite3_step(stmt) != SQLITE_DONE)
        sys_db_crash(db, "Failed to save mailbox key into the database (step)");

    if (key->id == 0)
        key->id = sqlite3_last_insert_rowid(db);

    sqlite3_finalize(stmt);
}

// Remove given key from the database
void db_mb_key_delete(sqlite3 *db, struct db_mb_key *key) {
    sqlite3_stmt *stmt;

    const char sql[] = "DELETE FROM mailbox_keys WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to delete mailbox key from database");

    if (sqlite3_bind_int(stmt, 1, key->id) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind mailbox key id, when deleting");

    if (sqlite3_step(stmt) != SQLITE_DONE)
        sys_db_crash(db, "Failed to delete mailbox key from database (step)");

    sqlite3_finalize(stmt);
}

// Process the next step of given statement and allocate or populate given object with row data
static struct db_mb_key * db_mb_key_process_row(sqlite3 *db, sqlite3_stmt *stmt, struct db_mb_key *dest) {
    int rc;
    struct db_mb_key *key = dest;

    // Check if there are no results or an error occurred
    if ((rc = sqlite3_step(stmt)) != SQLITE_ROW) {
        if (rc == SQLITE_DONE)
            return NULL;

        sys_db_crash(db, "Failed to fetch mailbox key (step)");
    }

    if (key == NULL)
        key = db_mb_key_new();

    key->id = sqlite3_column_int(stmt, 0);
    key->uses_left = sqlite3_column_int(stmt, 2);

    memcpy(key->key, sqlite3_column_blob(stmt, 1),
        min(MAILBOX_ACCESS_KEY_LEN, sqlite3_column_bytes(stmt, 1)));

    return key;
}

// Get mailbox key by id
struct db_mb_key * db_mb_key_get_by_pk(sqlite3 *db, int id, struct db_mb_key *dest) {
    sqlite3_stmt *stmt;
    struct db_mb_key *key;

    const char sql[] = "SELECT * FROM mailbox_keys WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch mailbox key from database (by pk)");

    if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind mailbox key id, when fetching");

    key = db_mb_key_process_row(db, stmt, dest);

    sqlite3_finalize(stmt);
    return key;
}

// Get mailbox key by key
struct db_mb_key * db_mb_key_get_by_key(sqlite3 *db, uint8_t *access_key, struct db_mb_key *dest) {
    sqlite3_stmt *stmt;
    struct db_mb_key *key;

    const char sql[] = "SELECT * FROM mailbox_keys WHERE key = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch mailbox key from database (by key)");

    if (sqlite3_bind_blob(stmt, 1, access_key, MAILBOX_ACCESS_KEY_LEN, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind mailbox key key, when fetching");

    key = db_mb_key_process_row(db, stmt, dest);

    sqlite3_finalize(stmt);
    return key;
}

// Pull new data from the database
void db_mb_key_refresh(sqlite3 *db, struct db_mb_key *key) {
    if (key->id == 0)
        return;

    db_mb_key_get_by_pk(db, key->id, key);
}

// Get all keys from the db, they are returned as array of pointers to
// mailbox key structures, n will be set to length of the array, if there
// are no keys in the db NULL will be returned
struct db_mb_key ** db_mb_key_get_all(sqlite3 *db, int *n) {
    int i;
    sqlite3_stmt *stmt;
    struct db_mb_key **keys;

    const char sql[] = "SELECT * FROM mailbox_keys";
    const char sql_count[] = "SELECT COUNT(*) FROM mailbox_keys";

    if (sqlite3_prepare_v2(db, sql_count, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to count all mailbox keys");

    if (sqlite3_step(stmt) != SQLITE_ROW)
        sys_db_crash(db, "Failed to count all mailbox keys (step)");

    *n = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (*n == 0) return NULL;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch all mailbox keys");

    keys = safe_malloc((sizeof(struct db_mb_key *) * (*n)), 
        "Failed to allocate memory for mailbox key list");

    for (i = 0; i < *n; i++) {
        keys[i] = db_mb_key_process_row(db, stmt, NULL);
    }

    sqlite3_finalize(stmt);
    return keys;
}

// Free mailbox key list fetched using db_mb_key_get_all()
void db_mb_key_free_all(struct db_mb_key **keys, int n) {
    int i;

    if (!keys)
        return;

    for (i = 0; i < n; i++)
        free(keys[i]);
    free(keys);
}