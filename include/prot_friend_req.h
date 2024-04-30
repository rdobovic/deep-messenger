#ifndef _INCLUDE_PROT_FRIEND_REQ_H_
#define _INCLUDE_PROT_FRIEND_REQ_H_

#include <onion.h>
#include <prot_main.h>
#include <db_contact.h>

// Friend request data structure
struct prot_friend_req {
    sqlite3 *db;
    struct db_contact *friend;
    struct prot_tran_handler htran;
    struct prot_recv_handler hrecv;
};

// Allocate new friend request handler
struct prot_friend_req * prot_friend_req_new(sqlite3 *db, const char *onion_address);

// Free memory for given friend request
void prot_friend_req_free(struct prot_friend_req *msg);

#endif