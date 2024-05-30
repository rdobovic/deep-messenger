#include <db_init.h>
#include <db_options.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <sys_crash.h>
#include <debug.h>

// Checks if row with given id exists, when has_value is set to 1 function 
// will return 1 only when option has_value, when it's set to 0 it will return 1
// if row exists even if it's set to NULL
static int db_options_row_exists(sqlite3 *db, const char *key, enum db_options_types type, int has_value) {
    int value;
    sqlite3_stmt *stmt;

    // Define SQL query for each option type
    const char *sql_row_exists[] = {
        // 0 => DB_OPTION_INT
        "SELECT COUNT(*) FROM options WHERE key = ?",
        // 1 => DB_OPTION_BIN
        "SELECT COUNT(*) FROM options WHERE key = ?",
        // 2 => DB_OPTION_TEXT
        "SELECT COUNT(*) FROM options WHERE key = ?"
    };

    // Define SQL query for each option type
    const char *sql_has_val[] = {
        // 0 => DB_OPTION_INT
        "SELECT COUNT(*) FROM options WHERE key = ? AND int_value IS NOT NULL",
        // 1 => DB_OPTION_BIN
        "SELECT COUNT(*) FROM options WHERE key = ? AND bin_value IS NOT NULL",
        // 2 => DB_OPTION_TEXT
        "SELECT COUNT(*) FROM options WHERE key = ? AND text_value IS NOT NULL"
    };

    if (sqlite3_prepare_v2(db, has_value ? sql_has_val[type] : sql_row_exists[type], -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch option count from db");

    if (sqlite3_bind_text(stmt, 1, key, -1, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind option key text when checking if defined");

    if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0) {
        sqlite3_finalize(stmt);
        return 1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

// Check if given option is defined for given type
int db_options_is_defined(sqlite3 *db, const char *key, enum db_options_types type) {
    return db_options_row_exists(db, key, type, 1);
}

// Fetch the option of int type
int db_options_get_int(sqlite3 *db, const char *key) {
    int value;
    sqlite3_stmt *stmt;

    const char sql[] = "SELECT int_value FROM options WHERE key = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch option of type int");

    if (sqlite3_bind_text(stmt, 1, key, -1, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind option key text when fetching int option");

    if (sqlite3_step(stmt) != SQLITE_ROW)
        return 0;

    value = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return value;
}

// Set value of in database option
void db_options_set_int(sqlite3 *db, const char *key, int value) {
    const char *sql;
    sqlite3_stmt *stmt;

    const char sql_update[] = "UPDATE options SET int_value = ? WHERE key = ?";
    const char sql_insert[] = "INSERT INTO options (int_value, key) VALUES (?, ?)";

    sql = db_options_row_exists(db, key, DB_OPTIONS_INT, 0) ? sql_update : sql_insert;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL))
        sys_db_crash(db, "Failed to set int option");

    if (sqlite3_bind_int(stmt, 1, value) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind int when setting int option value");
    
    if (sqlite3_bind_text(stmt, 2, key, -1, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind key when setting int option value");

    if (sqlite3_step(stmt) != SQLITE_DONE)
        sys_db_crash(db, "Failed to execute int option change");

    sqlite3_finalize(stmt);
}

// Fetch the option of binary object (BLOB) type
int db_options_get_bin(sqlite3 *db, const char *key, void *value, int value_len) {
    sqlite3_stmt *stmt;
    size_t db_value_len;
    const void *db_value;

    const char sql[] = "SELECT bin_value FROM options WHERE key = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch option of type binary");

    if (sqlite3_bind_text(stmt, 1, key, -1, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind key when fetching binary option value");

    if (sqlite3_step(stmt) != SQLITE_ROW)
        return 0;

    db_value = sqlite3_column_blob(stmt, 0);
    db_value_len = sqlite3_column_bytes(stmt, 0);

    memcpy(value, db_value, value_len < db_value_len ? value_len : db_value_len);
    sqlite3_finalize(stmt);
    return db_value_len;
}

void db_options_set_bin(sqlite3 *db, const char *key, const void *value, int value_len) {
    const char *sql;
    sqlite3_stmt *stmt;

    const char sql_update[] = "UPDATE options SET bin_value = ? WHERE key = ?";
    const char sql_insert[] = "INSERT INTO options (bin_value, key) VALUES (?, ?)";

    sql = db_options_row_exists(db, key, DB_OPTIONS_BIN, 0) ? sql_update : sql_insert;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL))
        sys_db_crash(db, "Failed to set binary option");

    if (sqlite3_bind_blob(stmt, 1, value, value_len, SQLITE_STATIC))
        sys_db_crash(db, "Failed to bind blob when setting binary option value");
    
    if (sqlite3_bind_text(stmt, 2, key, -1, NULL))
        sys_db_crash(db, "Failed to bind key when setting binary option value");

    if (sqlite3_step(stmt) != SQLITE_DONE)
        sys_db_crash(db, "Failed to execute binary option change");

    sqlite3_finalize(stmt);
}

// Fetch the option of text type
int db_options_get_text(sqlite3 *db, const char *key, char *value, int value_len) {
    sqlite3_stmt *stmt;
    size_t db_value_len;
    const char *db_value;

    const char sql[] = "SELECT text_value FROM options WHERE key = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to fetch option of type binary");

    if (sqlite3_bind_text(stmt, 1, key, -1, NULL) != SQLITE_OK)
        sys_db_crash(db, "Failed to bind key when fetching text option value");

    if (sqlite3_step(stmt) != SQLITE_ROW)
        return 0;

    db_value = sqlite3_column_text(stmt, 0);
    db_value_len = sqlite3_column_bytes(stmt, 0);

    debug("%d db_value_len", db_value_len);

    strncpy(value, db_value, 
        db_value_len < value_len ? db_value_len : value_len);
    sqlite3_finalize(stmt);
    return db_value_len;
}

void db_options_set_text(sqlite3 *db, const char *key, const char *value, int value_len) {
    const char *sql;
    sqlite3_stmt *stmt;

    const char sql_update[] = "UPDATE options SET text_value = ? WHERE key = ?";
    const char sql_insert[] = "INSERT INTO options (text_value, key) VALUES (?, ?)";

    sql = db_options_row_exists(db, key, DB_OPTIONS_TEXT, 0) ? sql_update : sql_insert;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL))
        sys_db_crash(db, "Failed to set text option");

    if (sqlite3_bind_text(stmt, 1, value, value_len, SQLITE_STATIC))
        sys_db_crash(db, "Failed to bind blob when setting text option value");
    
    if (sqlite3_bind_text(stmt, 2, key, -1, NULL))
        sys_db_crash(db, "Failed to bind key when setting text option value");

    if (sqlite3_step(stmt) != SQLITE_DONE)
        sys_db_crash(db, "Failed to execute text option change");

    sqlite3_finalize(stmt);
}