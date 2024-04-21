#ifndef _INCLUDE_DB_OPTIONS_H_
#define _INCLUDE_DB_OPTIONS_H_

#include <sqlite3.h>
#include <db_init.h>
#include <stdlib.h>

// Available option types
enum db_options_types {
    DB_OPTIONS_INT, DB_OPTIONS_BIN, DB_OPTIONS_TEXT
};

// Check if given option is defined for given type
int dbd_options_is_defined(sqlite3 *db, const char *key, enum db_options_types type);

#define db_options_is_defined(key, type) \
    dbd_options_is_defined(db_global_database, (key), (type))


// Fetch the option of int type
int dbd_options_get_int(sqlite3 *db, const char *key);

#define db_options_get_int(key) \
    dbd_options_get_int(db_global_database, (key))

// Set the value of int type option
void dbd_options_set_int(sqlite3 *db, const char *key, int value);

#define db_options_set_int(key, value) \
    dbd_options_set_int(db_global_database, (key), (value))

    
// Fetch the option of binary object (BLOB) type, returns number of bytes written
int dbd_options_get_bin(sqlite3 *db, const char *key, void *value, size_t value_len);

#define db_options_get_bin(key, value, value_len) \
    dbd_options_get_bin(db_global_database, (key), (value), (value_len))

// Set value of binary type option
void dbd_options_set_bin(sqlite3 *db, const char *key, const void *value, size_t value_len);

#define db_options_set_bin(key, value, value_len) \
    dbd_options_set_bin(db_global_database, (key), (value), (value_len))


// Fetch the option of text type
int dbd_options_get_text(sqlite3 *db, const char *key, char *value, size_t value_len);

#define db_options_get_text(key, value, value_len) \
    dbd_options_get_text(db_global_database, (key), (value), (value_len))

// Set value of text type option, value_len can be -1 for entire null terminated string
void dbd_options_set_text(sqlite3 *db, const char *key, const char *value, size_t value_len);

#define db_options_set_text(key, value, value_len) \
    dbd_options_set_text(db_global_database, (key), (value), (value_len))

#endif
