#ifndef _INCLUDE_PROT_MB_FATCH_H_
#define _INCLUDE_PROT_MB_FATCH_H_

#include <sqlite3.h>
#include <prot_main.h>
#include <constants.h>
#include <stdint.h>
#include <onion.h>

enum prot_mb_fetch_events {
    PROT_MB_FETCH_EV_OK   = 0x8701,
    PROT_MB_FETCH_EV_FAIL = 0x8702,
};

struct prot_mb_fetch {
    sqlite3 *db;

    // These values are pulled from the database and calculated
    char mb_onion_address[ONION_ADDRESS_LEN + 1];
    uint8_t mb_id[MAILBOX_ID_LEN];
    uint8_t mb_onion_key[ONION_ADDRESS_LEN];
    uint8_t mb_pub_sig_key[MAILBOX_ACCOUNT_KEY_PUB_LEN];
    uint8_t mb_priv_sig_key[MAILBOX_ACCOUNT_KEY_PRIV_LEN];

    struct prot_tran_handler htran;
    struct prot_recv_handler hrecv;
};

// Allocate new mailbox fetch handler object
struct prot_mb_fetch * prot_mb_fetch_new(sqlite3 *db);

// Free mailbox fetch handler
void prot_mb_fetch_free(struct prot_mb_fetch *msg);

#endif