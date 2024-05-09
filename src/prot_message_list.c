#include <sqlite3.h>
#include <prot_main.h>
#include <db_message.h>
#include <db_mb_message.h>
#include <prot_message_list.h>

// Called when message list is sent successfully
static void tran_done(struct prot_main *pmain, struct prot_tran_handler *phand) {

}

// Called to free memory taken by handler object when transmission is done
static void tran_cleanup(struct prot_tran_handler *phand) {
    
}

// Called to serilize message and put it into buffer
static void tran_setup(struct prot_main *pmain, struct prot_tran_handler *phand) {

}

// Called to free memeory taken by the handler object when message is processed
static void recv_cleanup(struct prot_recv_handler *phand) {

}

// Called to handle incomming message
static void recv_handle(struct prot_main *pmain, struct prot_recv_handler *phand) {

}

// Allocate new handler object
static struct prot_message_list prot_message_list_new(sqlite3 *db) {

}

// Allocate new message list handler (when in the client mode)
struct prot_message_list prot_message_list_client_new(sqlite3 *db, struct db_message **msgs, int n_msgs) {

}

// Allocate new message list handler (when in the mailbox mode)
struct prot_message_list prot_message_list_mailbox_new(sqlite3 *db, struct db_mb_message **msgs, int n_msgs) {

}

// Free given message list handler and messages given to new method
void prot_message_list_free(struct prot_message_list *msg) {

}