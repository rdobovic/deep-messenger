#ifndef _INCLUDE_PROT_MB_SET_CONTACTS_
#define _INCLUDE_PROT_MB_SET_CONTACTS_

#include <stdint.h>
#include <sqlite3.h>
#include <hooks.h>
#include <prot_main.h>
#include <db_contact.h>
#include <db_mb_account.h>
#include <db_mb_contact.h>

// Hook events, 
// hook data points to prot_mb_set_contacts message handler structure
enum prot_mb_set_contacts_events {
    // Displatched when contacts are set successfully
    PROT_MB_SET_CONTACTS_EV_OK,
    // Displatched when client failed to set contacts
    PROT_MB_SET_CONTACTS_EV_FAIL,
};

struct prot_mb_set_contacts {
    sqlite3 *db;
    struct hook_list *hooks;

    char onion_address[ONION_ADDRESS_LEN];     // Mailbox onion address
    uint8_t onion_pub_key[ONION_PUB_KEY_LEN];  // Onion address public key (ED25519)

    uint8_t cl_mb_id[MAILBOX_ID_LEN];                       // Used by client, their mailbox id
    uint8_t cl_sig_priv_key[MAILBOX_ACCOUNT_KEY_PRIV_LEN];  // Signing key for auth with the mailbox

    struct db_mb_account *mb_acc;    // Account data stored on mailbox

    int n_cl_conts;                  // Number of contacts to send (client side)
    struct db_contact **cl_conts;    // Contacts to send (client side)
    int n_mb_conts;                  // Number of received contacts (mailbox side)
    struct db_mb_contact **mb_conts; // Array of received contacts (mailbox side)

    struct prot_tran_handler htran;  // Standard transmission handler
    struct prot_recv_handler hrecv;  // Standard receive handler
};

// Allocate new set contacts handler, mailbox id and signing key should be provided
// only on the client side, on mailbox the should be NULL
struct prot_mb_set_contacts * prot_mb_set_contacts_new(
    sqlite3 *db,
    const char *onion_address,
    const uint8_t *cl_mb_id,
    const uint8_t *cl_sig_priv_key,
    struct db_contact **conts,
    int n_conts
);

// Free given set contacts message handler
void prot_mb_set_contacts_free(struct prot_mb_set_contacts *msg);

#endif