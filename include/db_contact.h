#ifndef _INCLUDE_DB_CONTACT_H_
#define _INCLUDE_DB_CONTACT_H_

#include <onion.h>
#include <stdint.h>
#include <sqlite3.h>

#define DB_CONTACT_NICK_MAX_LEN   255   // Defined in protocol definition

#define DB_CONTACT_SIG_KEY_PUB_LEN   32
#define DB_CONTACT_SIG_KEY_PRIV_LEN  64
#define DB_CONTACT_ENC_KEY_PUB_LEN   526
#define DB_CONTACT_ENC_KEY_PRIV_LEN  2348

enum db_contact_status {
    DB_CONTACT_ACTIVE,
    DB_CONTACT_PENDING_IN,
    DB_CONTACT_PENDING_OUT,
};

struct db_contact {
    // Internal user ID
    int id;
    // Current friend status (active, pending incomming/outgoing)
    enum db_contact_status status;
    // User nickname
    char nickname[DB_CONTACT_NICK_MAX_LEN + 1];
    // Onion address string
    char onion_address[ONION_ADDRESS_LEN + 1];
    
    // Key extracted from onion address string
    uint8_t onion_pub_key[ONION_KEY_LEN];

    // Keys generated during friend request
    uint8_t local_sig_key_pub[DB_CONTACT_SIG_KEY_PUB_LEN];
    uint8_t local_sig_key_priv[DB_CONTACT_SIG_KEY_PRIV_LEN];
    uint8_t local_enc_key_pub[DB_CONTACT_ENC_KEY_PUB_LEN];
    uint8_t local_enc_key_priv[DB_CONTACT_ENC_KEY_PRIV_LEN];

    // Keys received during friend request
    uint8_t remote_sig_key_pub[DB_CONTACT_SIG_KEY_PUB_LEN];
    uint8_t remote_enc_key_pub[DB_CONTACT_ENC_KEY_PUB_LEN];
};

// Create new empty contact object
struct db_contact * db_contact_new(void);

// Free memory for given contact, if you want to save changes
// you must first do so using save function
void db_contact_free(struct db_contact *cont);

// Save given contact to database
int db_contact_save(sqlite3 *db, struct db_contact *cont);

// They return NULL when not found

// Get contact by their local ID
struct db_contact * db_contact_get_by_pk(sqlite3 *db, int id);
// Get contact by ther onion address
struct db_contact * db_contact_get_by_onion(sqlite3 *db, char *onion_address);
// Get contact by ther remote signing key public
struct db_contact * db_contact_get_by_rsk_pub(sqlite3 *db, uint8_t *key);

#endif