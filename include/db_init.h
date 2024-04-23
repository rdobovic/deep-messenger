#ifndef _INLCUDE_DB_INIT_H_
#define _INLCUDE_DB_INIT_H_

#include <sqlite3.h>
#include <sys_crash.h>

// Global database object
extern sqlite3 *dbg;

#define CRASH_SOURCE_DB "Database error"
#define DB_OPEN_ERROR "Failed to open or create database file"

// Macro used to crash on fatal database errors and print database error message
#define sys_db_crash(db, error_desc) \
    sys_crash(CRASH_SOURCE_DB, "%s, with SQL error: %s", (error_desc), sqlite3_errmsg(db))

// Try to open database file on global database object
void db_init_global(const char *db_file_path);

// Create database schema
void db_init_schema(sqlite3 *db);

#endif