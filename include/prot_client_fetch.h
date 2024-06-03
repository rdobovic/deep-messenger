#ifndef _INCLUDE_PROT_CLIENT_FATCH_H_
#define _INCLUDE_PROT_CLIENT_FATCH_H_

#include <sqlite3.h>
#include <prot_main.h>
#include <db_contact.h>

enum prot_client_fetch_events {
    PROT_CLIENT_FETCH_EV_OK        = 0x8B01,
    PROT_CLIENT_FETCH_EV_FAIL      = 0x8B02,
    PROT_CLIENT_FETCH_EV_INCOMMING = 0x8B03,
};

struct prot_client_fetch {
    sqlite3 *db;
    struct db_contact *cont;

    struct prot_tran_handler htran;
    struct prot_recv_handler hrecv;
};

// Allocate new client fetch handler object
struct prot_client_fetch * prot_client_fetch_new(sqlite3 *db, struct db_contact *cont);

// Free client fetch handler
void prot_client_fetch_free(struct prot_client_fetch *msg);

#endif