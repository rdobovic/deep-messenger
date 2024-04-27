#ifndef _INCLUDE_DB_MB_KEY_H_
#define _INCLUDE_DB_MB_KEY_H_

#include <stdint.h>
#include <sqlite3.h>

#define DB_MB_KEY_LEN 25

struct db_mb_key {
    int id;
    uint8_t key[DB_MB_KEY_LEN];
    int uses_left;
};

// Create new empty key object
struct db_mb_key * db_mb_key_new(void);

// Free given key object, if you want to save it call the
// save function first
void db_mb_key_free(struct db_mb_key *key);

// Save changes on given object to database
void db_mb_key_save(sqlite3 *db, struct db_mb_key *key);

// Remove given key from the database
void db_mb_key_delete(sqlite3 *db, struct db_mb_key *key);

// Pull new data from the database
void db_mb_key_refresh(sqlite3 *db, struct db_mb_key *key);

// Get mailbox key by id
struct db_mb_key * db_mb_key_get_by_pk(sqlite3 *db, int id, struct db_mb_key *dest);

// Get mailbox key by key
struct db_mb_key * db_mb_key_get_by_key(sqlite3 *db, uint8_t *key, struct db_mb_key *dest);

// Get all keys from the db, they are returned as array of pointers to
// mailbox key structures, n will be set to length of the array, if there
// are no keys in the db NULL will be returned
struct db_mb_key ** db_mb_key_get_all(sqlite3 *db, int *n);

// Free mailbox key list fetched using db_mb_key_get_all()
void db_mb_key_free_all(struct db_mb_key **keys, int n);

#endif