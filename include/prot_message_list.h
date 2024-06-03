#ifndef _INCLUDE_PROT_MESSAGE_LIST_H_
#define _INCLUDE_PROT_MESSAGE_LIST_H_

#include <sqlite3.h>
#include <prot_main.h>
#include <db_message.h>
#include <db_mb_message.h>

// Message list can be sent as response to CLIENT FETCH and MAILBOX FETCH
// and will act a bit differentlly when processing the response depending
// on it's source
enum prot_message_list_from {
    PROT_MESSAGE_LIST_FROM_CLIENT,
    PROT_MESSAGE_LIST_FROM_MAILBOX,
};

// Data passed to hook callback
struct prot_message_list_ev_data {
    int n_messages;
    struct db_message **messages;
};

// Message list data
struct prot_message_list {
    uint8_t length;
    sqlite3 *db;
    enum prot_message_list_from from;

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

// When creating message receive handler use this function to set where is the
// message list comming from, is it from CLIENT or the MAILBOX, this is irelevant for transmission
void prot_message_list_from(struct prot_message_list *msg, enum prot_message_list_from from);

// Free given message list handler and messages given to new method
void prot_message_list_free(struct prot_message_list *msg);

#endif