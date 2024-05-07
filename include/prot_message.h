#ifndef _INCLUDE_PROT_MESSAGE_H_
#define _INCLUDE_PROT_MESSAGE_H_

#include <stdint.h>
#include <sqlite3.h>
#include <db_message.h>
#include <db_mb_message.h>
#include <prot_main.h>

struct prot_message {
    // If set to 1 hander will not try to send/wait ACK for message
    // it will just process it and store it
    int dry_run;
    sqlite3 *db;
    struct db_message *dbmsg_client;
    struct db_mb_message *dbmsg_mailbox;

    struct prot_tran_handler htran;
    struct prot_recv_handler hrecv;
};

// Allocate new object for message handler (actual text message)
struct prot_message * prot_message_client_new(sqlite3 *db, int dry_run, struct db_message *dbmsg);

// Allocate new object for message handler (actual text message)
struct prot_message * prot_message_mailbox_new(sqlite3 *db, int dry_run, struct db_mb_message *dbmsg);

// Free given handler
void prot_message_free(struct prot_message *msg);

#endif