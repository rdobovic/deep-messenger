#include <hooks.h>
#include <prot_main.h>
#include <prot_friend_req.h>
#include <db_contact.h>
#include <db_message.h>
#include <prot_message.h>
#include <prot_message_list.h>
#include <prot_client_fetch.h>
#include <prot_mb_fetch.h>
#include <prot_transaction.h>
#include <debug.h>
#include <app.h>

// Handle incomming friend request
static void hook_friend_req(int ev, void *data, void *cbarg) {
    struct app_data *app = cbarg;
    struct db_contact *friend = data;

    if (friend->deleted) return;

    if (friend->status == DB_CONTACT_ACTIVE) {
        app_ui_info(app, "[Friend] Contact [%s] %s accepted your friend request",
            friend->nickname, friend->onion_address);
        app_update_contacts(app);
        ui_stack_redraw(app->ui.stack);
    }
    if (friend->status == DB_CONTACT_PENDING_IN) {
        app_ui_info(app, "[Friend] Contact [%s] %s sent you a friend request",
            friend->nickname, friend->onion_address);
    }
}

// Handle incomming message
static void hook_message(int ev, void *data, void *cbarg) {
    struct app_data *app = cbarg;
    struct db_message *msg = data;
    struct db_contact *cont;
    debug("GOT NEW MESSAGE");

    if (app->cont_selected && app->cont_selected->id == msg->contact_id) {
        if (msg->type == DB_MESSAGE_TEXT)
            app_ui_chat_refresh(app, 0);
        if (msg->type == DB_MESSAGE_RECV)
            app_ui_chat_refresh(app, 1);
    }

    debug("GOT NEW MESSAGE => REFRESH DONE");

    cont = db_contact_get_by_pk(app->db, msg->contact_id, NULL);
    debug("GOT NEW MESSAGE => GOT CONTACT %p", cont);
    if (msg->type == DB_MESSAGE_TEXT) {
        app_ui_info(app, "[Message] You have new message from [%s]", cont->nickname);
    }
    if (msg->type == DB_MESSAGE_NICK) {
        app_update_contacts(app);
        ui_stack_redraw(app->ui.stack);
        app_ui_info(app, "[Message] Your friend [%s] changed their nickname, refreshing UI", cont->nickname);
    }

    debug("GOT NEW MESSAGE => FINISHED PROCESSING");

    db_contact_free(cont);
}

// Handle incomming CLIENT FETCH request
static void hook_client_fetch(int ev, void *data, void *cbarg) {
    struct app_data *app = cbarg;
    struct prot_message_list_ev_data *evdata = data;

    if (evdata->n_messages > 0 && evdata->messages[0]->contact_id == app->cont_selected->id) {
        app_ui_chat_refresh(app, 1);
    }
}

// Add hooks to main protocol handler for incomming connection
void app_pmain_add_hooks(struct app_data *app, struct prot_main *pmain) {
    // Add all hooks above
    hook_add(pmain->hooks, PROT_FRIEND_REQ_EV_INCOMMING, hook_friend_req, app);
    hook_add(pmain->hooks, PROT_MESSAGE_EV_INCOMMING, hook_message, app);
    hook_add(pmain->hooks, PROT_CLIENT_FETCH_EV_INCOMMING, hook_client_fetch, app);
}

// Handle contact sync response
static void hook_contact_sync(int ev, void *data, void *cbarg) {
    int i, ref_chat = 0, ref_contacts = 0;
    struct app_data *app = cbarg;
    struct db_contact *cont;
    struct prot_message_list_ev_data *evdata = data;

    if (ev == PROT_CLIENT_FETCH_EV_FAIL)
        return;

    if (evdata->n_messages == 0)
        return;

    for (i = 0; i < evdata->n_messages; i++) {
        // If someone changed their nickname refresh UI
        if (evdata->messages[i]->type == DB_MESSAGE_NICK) {
            ref_contacts = 1;
        }
        // If this is message for opened chat refresh the chat
        if (app->cont_selected && app->cont_selected->id == evdata->messages[i]->contact_id) {
            ref_chat = 1;
        }
    }

    cont = db_contact_get_by_pk(app->db, evdata->messages[0]->contact_id, NULL);
    app_ui_info(app, "[Message] Fetched %d new message(s) from [%s]", 
        evdata->n_messages, cont->nickname);

    if (ref_chat) {
        app_ui_chat_refresh(app, 0);
    }
    if (ref_contacts) {
        app_update_contacts(app);
        ui_stack_redraw(app->ui.stack);
        app_ui_info(app, "[Message] Your friend [%s] changed their nickname, refreshing UI", cont->nickname);
    }
    db_contact_free(cont);
}

// Sync messages with given contact
void app_contact_sync(struct app_data *app, struct db_contact *cont) {
    int n_msgs, i;
    struct db_message **msgs;
    struct prot_main *pmain;
    struct prot_txn_req *treq;
    struct prot_client_fetch *clfet;

    pmain = prot_main_new(app->base, app->db);
    treq = prot_txn_req_new();
    clfet = prot_client_fetch_new(app->db, cont);

    prot_main_free_on_done(pmain, 1);
    hook_add(pmain->hooks, PROT_CLIENT_FETCH_EV_OK, hook_contact_sync, app);
    hook_add(pmain->hooks, PROT_CLIENT_FETCH_EV_FAIL, hook_contact_sync, app);
    prot_main_push_tran(pmain, &(treq->htran));

    // Send all undelivered messages
    msgs = db_message_get_all(app->db, cont, DB_MESSAGE_STATUS_UNDELIVERED, &n_msgs);
    for (i = 0; i < n_msgs; i++) {
        struct prot_message *msg;
        msg = prot_message_to_client_new(app->db, msgs[i]);
        prot_main_push_tran(pmain, &(msg->htran));
    }
    free(msgs);  // Free just array, not messages

    // Send RECV for all unconfirmed messages
    msgs = db_message_get_all(app->db, cont, DB_MESSAGE_STATUS_RECV, &n_msgs);
    for (i = 0; i < n_msgs; i++) {
        struct db_message *recvmsg;
        struct prot_message *msg;

        recvmsg = db_message_new();
        recvmsg->type = DB_MESSAGE_RECV;
        recvmsg->contact_id = cont->id;
        recvmsg->sender = DB_MESSAGE_SENDER_ME;
        recvmsg->status = DB_MESSAGE_STATUS_UNDELIVERED;
        db_message_gen_id(recvmsg);
        memcpy(recvmsg->body_recv_id, msgs[i]->global_id, MESSAGE_ID_LEN);

        msg = prot_message_to_client_new(app->db, recvmsg);
        prot_main_push_tran(pmain, &(msg->htran));
    }
    db_message_free_all(msgs, n_msgs);

    prot_main_push_tran(pmain, &(clfet->htran));
    prot_main_connect(pmain, cont->onion_address,
        app->cf.app_port, "127.0.0.1", app->cf.tor_port);
}

// Handle mb sync response
static void hook_mb_sync(int ev, void *data, void *cbarg) {
    struct app_data *app = cbarg;
    struct prot_message_list_ev_data *evdata = data;
    int i, ref_chat = 0, ref_contacts = 0;

    if (ev == PROT_MB_FETCH_EV_FAIL) {
        return;
    }

    app_ui_info(app, "[Message] Fetched %d new messages from the mailbox", evdata->n_messages);

    for (i = 0; i < evdata->n_messages; i++) {
        // If someone changed their nickname refresh UI
        if (evdata->messages[i]->type == DB_MESSAGE_NICK) {
            ref_contacts = 1;
        }
        // If this is message for opened chat refresh the chat
        if (app->cont_selected && app->cont_selected->id == evdata->messages[i]->contact_id) {
            ref_chat = 1;
        }
    }

    if (ref_chat) {
        app_ui_chat_refresh(app, 0);
    }
    if (ref_contacts) {
        app_update_contacts(app);
        app_ui_info(app, "[Message] Someone changed their nickname, refreshing UI");
    }
}

// Sync messages from your mailbox account
void app_mailbox_sync(struct app_data *app) {
    struct prot_main *pmain;
    struct prot_txn_req *treq;
    struct prot_mb_fetch *mbfet;

    pmain = prot_main_new(app->base, app->db);
    treq = prot_txn_req_new();
    mbfet = prot_mb_fetch_new(app->db);

    prot_main_free_on_done(pmain, 1);
    hook_add(pmain->hooks, PROT_MB_FETCH_EV_OK, hook_mb_sync, app);
    hook_add(pmain->hooks, PROT_MB_FETCH_EV_FAIL, hook_mb_sync, app);
    prot_main_push_tran(pmain, &(treq->htran));
    prot_main_push_tran(pmain, &(mbfet->htran));

    prot_main_connect(pmain, mbfet->mb_onion_address,
        app->cf.mailbox_port, "127.0.0.1", app->cf.tor_port);
}