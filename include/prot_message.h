#ifndef _INCLUDE_PROT_MESSAGE_H_
#define _INCLUDE_PROT_MESSAGE_H_

#include <stdint.h>
#include <sqlite3.h>
#include <db_message.h>
#include <db_mb_message.h>
#include <prot_main.h>

enum prot_message_to {
    PROT_MESSAGE_TO_CLIENT,
    PROT_MESSAGE_TO_MAILBOX
};

struct prot_message {
    sqlite3 *db;
    // Used to specify if message if we are sending message to the
    // or the client
    enum prot_message_to to;
    struct db_message *dbmsg_client;
    struct db_mb_message *dbmsg_mailbox;

    struct prot_tran_handler htran;
    struct prot_recv_handler hrecv;
};

// Allocate new object for message handler (actual text message)
struct prot_message * prot_message_client_new(sqlite3 *db, enum prot_message_to to, struct db_message *dbmsg);

// Allocate new object for message handler (actual text message)
struct prot_message * prot_message_mailbox_new(sqlite3 *db, struct db_mb_message *dbmsg);

// Free given handler
void prot_message_free(struct prot_message *msg);

#endif