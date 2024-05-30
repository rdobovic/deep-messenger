#ifndef _INCLUDE_PROT_MESSAGE_H_
#define _INCLUDE_PROT_MESSAGE_H_

#include <stdint.h>
#include <sqlite3.h>
#include <db_message.h>
#include <db_mb_message.h>
#include <prot_main.h>
#include <hooks.h>

enum prot_message_to {
    PROT_MESSAGE_TO_CLIENT,
    PROT_MESSAGE_TO_MAILBOX,
};

struct prot_message {
    sqlite3 *db;
    struct hook_list *hooks;
    enum prot_message_to to;

    struct db_message *client_msg;
    struct db_contact *client_cont;

    struct db_mb_message *mailbox_msg;

    struct prot_tran_handler htran;
    struct prot_recv_handler hrecv;
};

// Allocate new message handler for sending message between clients
struct prot_message * prot_message_to_client_new(sqlite3 *db, struct db_message *dbmsg);

// Allocate new message handler for sending message between client and mailbox
struct prot_message * prot_message_to_mailbox_new(sqlite3 *db, struct db_message *dbmsg);

// Free given handler and message model given to the new method
void prot_message_free(struct prot_message *msg);

#endif