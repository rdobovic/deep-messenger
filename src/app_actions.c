#include <hooks.h>
#include <prot_main.h>
#include <prot_friend_req.h>
#include <db_contact.h>
#include <db_message.h>
#include <prot_message.h>
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
        app_ui_info(app, "[Message] Your friend [%s] changed his nickname, refreshing UI", cont->nickname);
        app_update_contacts(app);
    }

    debug("GOT NEW MESSAGE => FINISHED PROCESSING");

    db_contact_free(cont);
}

// Add hooks to main protocol handler for incomming connection
void app_pmain_add_hooks(struct app_data *app, struct prot_main *pmain) {
    // Add all hooks above
    hook_add(pmain->hooks, PROT_FRIEND_REQ_EV_INCOMMING, hook_friend_req, app);
    hook_add(pmain->hooks, PROT_MESSAGE_EV_INCOMMING, hook_message, app);
}