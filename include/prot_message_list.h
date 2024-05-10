#ifndef _INCLUDE_PROT_MESSAGE_LIST_H_
#define _INCLUDE_PROT_MESSAGE_LIST_H_

#include <sqlite3.h>
#include <prot_main.h>
#include <db_message.h>
#include <db_mb_message.h>

// Message list data
struct prot_message_list {
    uint8_t length;
    sqlite3 *db;

    struct db_contact *client_cont;

    int n_client_msgs;
    struct db_message **client_msgs;
    int n_mailbox_msgs;
    struct db_mb_message **mailbox_msgs;

    struct prot_tran_handler htran;
    struct prot_recv_handler hrecv;
};

// Allocate new message list handler (when in the client mode)
struct prot_message_list * prot_message_list_client_new(
    sqlite3 *db, struct db_contact *cont, struct db_message **msgs, int n_msgs);

// Allocate new message list handler (when in the mailbox mode)
struct prot_message_list * prot_message_list_mailbox_new(sqlite3 *db, struct db_mb_message **msgs, int n_msgs);

// Free given message list handler and messages given to new method
void prot_message_list_free(struct prot_message_list *msg);

#endif