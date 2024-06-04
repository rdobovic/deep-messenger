#include <ui_prompt.h>
#include <db_contact.h>
#include <db_message.h>
#include <prot_main.h>
#include <prot_message.h>
#include <prot_transaction.h>
#include <hooks.h>
#include <app.h>
#include <debug.h>

// Message sending hook callback
static void message_mb_hook_cb(int ev, void *data, void *cbarg) {
    struct app_data *app = cbarg;
    struct db_message *dbmsg = data;

    if (ev == PROT_MESSAGE_EV_OK) {
        if (app->cont_selected && app->cont_selected->id == dbmsg->contact_id)
            app_ui_chat_refresh(app, 1);
    }
}

// Try to send message to the contact mailbox (makes a copy of provided message)
void app_message_send_mb(struct app_data *app, const struct db_message *msg) {
    struct prot_main *pmain;
    struct prot_txn_req *treq;
    struct prot_message *pmsg;
    struct db_message *dbmsg;
    struct db_contact *dbcont;

    dbcont = db_contact_get_by_pk(app->db, msg->contact_id, NULL);
    if (!dbcont || !dbcont->has_mailbox) {
        if (app->cf.mb_direct)
            app_ui_info(app, "[Message] mbdirect: Cannot send "
                "message to the mailbox, client has no mailbox");
        db_contact_free(dbcont);
        return;
    }
    dbmsg = db_message_get_by_pk(app->db, msg->id, NULL);

    pmain = prot_main_new(app->base, app->db);
    treq = prot_txn_req_new();
    pmsg = prot_message_to_mailbox_new(app->db, dbmsg);

    prot_main_free_on_done(pmain, 1);
    hook_add(pmain->hooks, PROT_MESSAGE_EV_OK, message_mb_hook_cb, app);
    hook_add(pmain->hooks, PROT_MESSAGE_EV_FAIL, message_mb_hook_cb, app);
    prot_main_push_tran(pmain, &(treq->htran));
    prot_main_push_tran(pmain, &(pmsg->htran));

    prot_main_connect(pmain, dbcont->mailbox_onion,
        app->cf.mailbox_port, "127.0.0.1", app->cf.tor_port);
    db_contact_free(dbcont);
}

// Message sending hook callback
static void message_hook_cb(int ev, void *data, void *cbarg) {
    struct app_data *app = cbarg;
    struct db_message *dbmsg = data;

    if (ev == PROT_MESSAGE_EV_OK) {
        if (app->cont_selected && app->cont_selected->id == dbmsg->contact_id)
            app_ui_chat_refresh(app, 1);
        return;
    }

    // Send message to the mailbox if online
    app_message_send_mb(app, dbmsg);
}

// Send message to associated contact (frees message by itself)
void app_message_send(struct app_data *app, struct db_message *dbmsg) {
    struct prot_main *pmain;
    struct prot_txn_req *treq;
    struct prot_message *pmsg;
    struct db_contact *cont;

    cont = db_contact_get_by_pk(app->db, dbmsg->contact_id, NULL);
    pmain = prot_main_new(app->base, app->db);
    treq = prot_txn_req_new();
    pmsg = prot_message_to_client_new(app->db, dbmsg);

    prot_main_free_on_done(pmain, 1);
    hook_add(pmain->hooks, PROT_MESSAGE_EV_OK, message_hook_cb, app);
    hook_add(pmain->hooks, PROT_MESSAGE_EV_FAIL, message_hook_cb, app);
    prot_main_push_tran(pmain, &(treq->htran));
    prot_main_push_tran(pmain, &(pmsg->htran));

    prot_main_connect(pmain, cont->onion_address,
        app->cf.app_port, "127.0.0.1", app->cf.tor_port);
    db_contact_free(cont);
}

// Handle config shell commands
void app_ui_handle_msg(struct ui_prompt *prt, void *att) {
    struct app_data *app = att;
    struct db_message *dbmsg;
    struct prot_main *pmain;
    struct prot_txn_req *treq;
    struct prot_message *msg;
    
    app_ui_info(app, "[Message] Attempting to send message to [%s]", app->cont_selected->nickname);
    ui_prompt_clear(prt);

    dbmsg = db_message_new();
    dbmsg->contact_id = app->cont_selected->id;
    dbmsg->type = DB_MESSAGE_TEXT;
    dbmsg->sender = DB_MESSAGE_SENDER_ME;
    dbmsg->status = DB_MESSAGE_STATUS_UNDELIVERED;
    db_message_gen_id(dbmsg);
    db_message_set_text(dbmsg, ui_prompt_get_input(prt), -1);
    db_message_save(app->db, dbmsg);

    app_ui_chat_refresh(app, 0);

    if (app->cf.mb_direct) {
        app_message_send_mb(app, dbmsg);
        db_message_free(dbmsg);
        return;
    }

    app_message_send(app, dbmsg);
}