#include <ui_prompt.h>
#include <db_contact.h>
#include <db_message.h>
#include <prot_main.h>
#include <prot_message.h>
#include <prot_transaction.h>
#include <hooks.h>
#include <app.h>

// Message sending hook callback
static void message_hook_cb(int ev, void *data, void *cbarg) {
    struct app_data *app = cbarg;
    struct db_message *dbmsg = data;

    if (ev == PROT_MESSAGE_EV_OK) {
        if (app->cont_selected && app->cont_selected->id == dbmsg->contact_id)
            app_ui_chat_refresh(app, 1);
    }

    // Try sending to the mailbox...
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

    pmain = prot_main_new(app->base, app->db);
    treq = prot_txn_req_new();
    msg = prot_message_to_client_new(app->db, dbmsg);

    prot_main_free_on_done(pmain, 1);
    hook_add(pmain->hooks, PROT_MESSAGE_EV_OK, message_hook_cb, app);
    hook_add(pmain->hooks, PROT_MESSAGE_EV_FAIL, message_hook_cb, app);
    prot_main_push_tran(pmain, &(treq->htran));
    prot_main_push_tran(pmain, &(msg->htran));

    prot_main_connect(pmain, app->cont_selected->onion_address,
        app->cf.app_port, "127.0.0.1", app->cf.tor_port);
}