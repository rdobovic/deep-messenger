#include <stdint.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <db_message.h>
#include <db_mb_message.h>
#include <sys_memory.h>
#include <prot_main.h>
#include <prot_message.h>

// Called if message is sent successfully
static void tran_done(struct prot_main *pmain, struct prot_tran_handler *phand) {

}

// Free message handler object
static void tran_cleanup(struct prot_tran_handler *phand) {

}

// Called to put message into buffer
static void tran_setup(struct prot_main *pmain, struct prot_tran_handler *phand) {

}

// Free message handler object
static void recv_cleanup(struct prot_recv_handler *phand) {

}

// Handler incomming message
static void recv_handle(struct prot_main *pmain, struct prot_recv_handler *phand) {

}

// Allocate new message object
static struct prot_message * prot_message_new(sqlite3 *db, int dry_run) {
    struct prot_message *msg;

    msg = safe_malloc(sizeof(struct prot_message), 
        "Failed to allocate message protocol handler");
    memset(msg, 0, sizeof(struct prot_message));

}

// Allocate new object for message handler (actual text message)
struct prot_message * prot_message_client_new(sqlite3 *db, int dry_run, struct db_message *dbmsg) {
    struct prot_message *msg;

    msg = safe_malloc(sizeof(struct prot_message), 
        "Failed to allocate message protocol handler");
    memset(msg, 0, sizeof(struct prot_message));

    msg->db = db;
    msg->dry_run = dry_run;
    msg->dbmsg_client = dbmsg;
}

// Allocate new object for message handler (actual text message)
struct prot_message * prot_message_mailbox_new(sqlite3 *db, int dry_run, struct db_mb_message *dbmsg) {

}

void prot_message_free(struct prot_message *msg) {

}