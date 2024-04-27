#ifndef _INCLUDE_DB_MB_ACCOUNT_
#define _INCLUDE_DB_MB_ACCOUNT_

#include <db_contact.h>
#include <stdint.h>
#include <sqlite3.h>

#define DB_MB_ACCOUNT_SIG_KEY_PUB_LEN  32
#define DB_MB_ACCOUNT_SIG_KEY_PRIV_LEN 64

struct db_mb_account {
    int id;
    uint8_t mailbox_id[DB_CONTACT_MAILBOX_ID_LEN];
    uint8_t signing_pub_key[DB_MB_ACCOUNT_SIG_KEY_PUB_LEN];
};

// Create new empty account object
struct db_mb_account * db_mb_account_new(void);

// Free given account object, if you want to save it
// call the save function first
void db_mb_account_free(struct db_mb_account *acc);

// Save changes on given object to the database
void db_mb_account_save(sqlite3 *db, struct db_mb_account *acc);

// Delete given malilbox account from the database
void db_mb_account_delete(sqlite3 *db, struct db_mb_account *acc);

// Pull new data from the database
void db_mb_account_refresh(sqlite3 *db, struct db_mb_account *acc);

// They return NULL when not found, if dest is not NULL functions will copy
// data from the database into dest, otherwise they will allocate new structure

// Get mailbox account by id
struct db_mb_account * db_mb_account_get_by_pk(sqlite3 *db, int id, struct db_mb_account *dest);

// Get mailbox account by unique mailbox id
struct db_mb_account * db_mb_account_get_by_mbid(sqlite3 *db, uint8_t *mbid, struct db_mb_account *dest);

#endif