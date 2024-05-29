#ifndef _INCLUDE_PROT_MB_ACCOUNT_
#define _INCLUDE_PROT_MB_ACCOUNT_

#include <stdint.h>
#include <sqlite3.h>
#include <hooks.h>
#include <prot_main.h>
#include <db_mb_account.h>

// Hook events for mailbox account actions,
// hook data points to prot_mb_acc_data for new account
enum prot_mb_acc_events {
    PROT_MB_ACC_DELETE_EV_OK     = 0x8901,
    PROT_MB_ACC_DELETE_EV_FAIL   = 0x8902,
    PROT_MB_ACC_REGISTER_EV_OK   = 0x8501,
    PROT_MB_ACC_REGISTER_EV_FAIL = 0x8502,
};

// Data used during account registration on client side
struct prot_mb_acc_data {
    char onion_address[ONION_ADDRESS_LEN];               // Mailbox onion address
    uint8_t onion_key[ONION_PRIV_KEY_LEN];               // PUB key extracted from Mailbox onion
    uint8_t access_key[MAILBOX_ACCESS_KEY_LEN];          // Mailbox access key
    uint8_t sig_pub_key[MAILBOX_ACCOUNT_KEY_PUB_LEN];    // Generated public key
    uint8_t sig_priv_key[MAILBOX_ACCOUNT_KEY_PRIV_LEN];  // Generated private key
    uint8_t mailbox_id[MAILBOX_ID_LEN];                  // Received mailbox id
};

// Mailbox account registration request
struct prot_mb_acc {
    sqlite3 *db;

    // Used when request arrives to mailbox (on mailbox)
    struct db_mb_account *mb_acc;
    // Used when sending request (on client)
    struct prot_mb_acc_data *cl_acc;

    struct prot_tran_handler htran;
    struct prot_recv_handler hrecv;
};

// Create new mailbox register request handler, arguments are needed for transmission
// for receiver they should be set to NULL
struct prot_mb_acc * prot_mb_acc_register_new(sqlite3 *db, const char *onion_address, const uint8_t *access_key);

// Free given mailbox register handler
void prot_mb_acc_register_free(struct prot_mb_acc *msg);

// Create new account granted response handler,
// creates response for given registration request
struct prot_mb_acc * prot_mb_acc_granted_new(struct prot_mb_acc *acc_reg_req);

// Free given mailbox account granted response
void prot_mb_acc_granted_free(struct prot_mb_acc *msg);

// Create new mailbox delete account handler, by sending this message client
// deletes its account and all associated messages
struct prot_mb_acc * prot_mb_acc_delete_new(sqlite3 *db, const char *onion_address, const uint8_t *mb_id, const uint8_t *mb_sig_priv_key);

// Free given mailbox account delete handler
void prot_mb_acc_delete_free(struct prot_mb_acc *msg);

#endif