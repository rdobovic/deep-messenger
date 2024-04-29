#ifndef _INCLUDE_DB_MB_CONTACT_H_
#define _INCLUDE_DB_MB_CONTACT_H_

#include <stdint.h>
#include <sqlite3.h>
#include <db_contact.h>
#include <db_mb_account.h>
#include <constants.h>

struct db_mb_contact {
    int id;
    int account_id;
    uint8_t signing_pub_key[CLIENT_SIG_KEY_PUB_LEN];
};

// Create new empty contact object
struct db_mb_contact *db_mb_contact_new(void);

// Free given contact object, if you want to save it call
// the save function first
void db_mb_contact_free(struct db_mb_contact *cont);

// Save changes on given object to the database
void db_mb_contact_save(sqlite3 *db, struct db_mb_contact *cont);

// Remove given contact from the database
void db_mb_contact_delete(sqlite3 *db, struct db_mb_contact *cont);

// Remove all contacts from database for given account
void db_mb_contact_delete_all(sqlite3 *db, struct db_mb_account *acc);

// Pull new data from the database
void db_mb_contact_refresh(sqlite3 *db, struct db_mb_contact *cont);

// Get mailbox contact by id
struct db_mb_contact * db_mb_contact_get_by_pk(sqlite3 *db, int id, struct db_mb_contact *dest);

// Get mailbox contact by associated account and signing key
struct db_mb_contact * db_mb_contact_get_by_acc_and_key(
    sqlite3 *db, struct db_mb_account *acc, uint8_t *key, struct db_mb_contact *dest);

#endif