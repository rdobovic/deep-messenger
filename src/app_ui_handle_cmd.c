#include <ui_prompt.h>
#include <ui_logger.h>
#include <string.h>
#include <array.h>
#include <cmd_parse.h>
#include <constants.h>
#include <onion.h>
#include <db_options.h>
#include <debug.h>
#include <base32.h>

#include <prot_main.h>
#include <db_message.h>
#include <prot_transaction.h>
#include <prot_mb_account.h>
#include <prot_friend_req.h>
#include <prot_mb_set_contacts.h>

#include <app.h>

// Print help message to the console
static void command_help(int argc, char **argv, void *cbarg) {
    struct app_data *app = cbarg;

    app_ui_shell(app, "List of all available commands:");
    app_ui_shell(app, "  help                Prints this help message");
    app_ui_shell(app, "  mailboxfetch        Fetch new messages from mailbox server");
    app_ui_shell(app, "  fetch <onion>       Fetch new messages from given client");
    app_ui_shell(app, "  friends             List all friends");
    app_ui_shell(app, "  friendadd <onion>   Send friend request to add a new friend");
    app_ui_shell(app, "  friendrm <onion>    Remove given friend");
    app_ui_shell(app, "  info                Print your account info");
    app_ui_shell(app, "  nickname <nick>     Change your nickname to <nick>");
    app_ui_shell(app, "  mbreg <onion> <key> Register to given mailbox server");
    app_ui_shell(app, "  mbrm                Delete account on your current mailbox server");
    app_ui_shell(app, "  version             Prints app and protocol version");
}

// Print application version to the console
static void command_version(int argc, char **argv, void *cbarg) {
    struct app_data *app = cbarg;

    app_ui_shell(app, "Deep Messenger version %s (protocol v%d)",
        DEEP_MESSENGER_APP_VERSION, DEEP_MESSENGER_PROTOCOL_VER);
    app_ui_shell(app, "Made by rdobovic");
}

// Print account information to the console
static void command_info(int argc, char **argv, void *cbarg) {
    struct app_data *app = cbarg;

    int has_mailbox;
    char nick[CLIENT_NICK_MAX_LEN + 1];
    char onion_address[ONION_ADDRESS_LEN + 1];
    char mb_onion_address[ONION_ADDRESS_LEN + 1];
    
    db_options_get_text(app->db, "client_nickname", nick, sizeof(nick));
    db_options_get_text(app->db, "onion_address", onion_address, sizeof(onion_address));

    has_mailbox = db_options_is_defined(app->db, "client_mailbox_onion_address", DB_OPTIONS_TEXT);
    if (has_mailbox)
        db_options_get_text(app->db, "client_mailbox_onion_address", mb_onion_address, sizeof(mb_onion_address));

    app_ui_shell(app, "Your account information:");
    app_ui_shell(app, "  nickname:              %s", nick);
    app_ui_shell(app, "  your onion address:    %s", onion_address);
    app_ui_shell(app, "  mailbox onion address: %s", 
        has_mailbox ? mb_onion_address : "(you have no mailbox)");
}

// Handle account registration status on pmain
static void command_mbreg_hook_cb(int ev, void *data, void *cbarg) {
    int i;
    struct app_data *app = cbarg;
    struct prot_mb_acc_data *acc = data;

    if (ev != PROT_MB_ACC_REGISTER_EV_OK) {
        app_ui_shell(app, "error: Mailbox registration failed");
        return;
    }

    db_options_set_bin(app->db, "client_mailbox_id", acc->mailbox_id, MAILBOX_ID_LEN);
    db_options_set_bin(app->db, "client_mailbox_sig_pub_key", acc->sig_pub_key, MAILBOX_ACCOUNT_KEY_PUB_LEN);
    db_options_set_bin(app->db, "client_mailbox_sig_priv_key", acc->sig_priv_key, MAILBOX_ACCOUNT_KEY_PRIV_LEN);
    db_options_set_text(app->db, "client_mailbox_onion_address", acc->onion_address, ONION_ADDRESS_LEN);

    app_ui_shell(app, "Successfully registered the mailbox server");

    for (i = 0; i < app->n_contacts; i++) {
        struct db_message *msg;

        if (app->contacts[i]->deleted || app->contacts[i]->status != DB_CONTACT_ACTIVE)
            continue;

        msg = db_message_new();
        msg->type = DB_MESSAGE_MBOX;
        msg->contact_id = app->contacts[i]->id;
        msg->sender = DB_MESSAGE_SENDER_ME;
        msg->status = DB_MESSAGE_STATUS_UNDELIVERED;
        db_message_gen_id(msg);

        memcpy(msg->body_mbox_id, acc->mailbox_id, MAILBOX_ID_LEN);
        memcpy(msg->body_mbox_onion, acc->onion_address, ONION_ADDRESS_LEN);
        db_message_save(app->db, msg);
        app_message_send(app, msg);
    }
}

// Attempt to register mailbox account
static void command_mbreg(int argc, char **argv, void *cbarg) {
    struct app_data *app = cbarg;

    struct prot_main *pmain;
    struct prot_mb_acc *accreg;
    struct prot_txn_req *txnreq;

    int key_len;
    uint8_t *access_key;

    if (db_options_is_defined(app->db, "client_mailbox_id", DB_OPTIONS_BIN)) {
        app_ui_shell(app, "error: You already have mailbox account");
        return;
    }
        
    if (!onion_address_valid(argv[1])) {
        app_ui_shell(app, "error: Invalid mailbox onion address provided");
        return;
    }
    if (!base32_valid(argv[2], -1)) {
        app_ui_shell(app, "error: Mailbox access key must be valid base32 coded string");
        return;
    }

    // Decode mailbox access key
    access_key = array(uint8_t);
    array_expand(access_key, BASE32_DECODED_LEN(strlen(argv[2])));
    key_len = base32_decode(argv[2], strlen(argv[2]), access_key);
    if (key_len != MAILBOX_ACCESS_KEY_LEN) {
        app_ui_shell(app, "error: Mailbox access key does not have valid key length");
        array_free(access_key);
        return;
    }

    app_ui_shell(app, "Attempting registration on the mailbox server");
    app_ui_shell(app, "  server:     %s", argv[1]);
    app_ui_shell(app, "  access key: %s", argv[2]);
    app_ui_shell(app, "This could take some time...");

    pmain = prot_main_new(app->base, app->db);
    txnreq = prot_txn_req_new();
    accreg = prot_mb_acc_register_new(app->db, argv[1], access_key);
    array_free(access_key);

    prot_main_free_on_done(pmain, 1);
    hook_add(pmain->hooks, PROT_MB_ACC_REGISTER_EV_OK, command_mbreg_hook_cb, app);
    hook_add(pmain->hooks, PROT_MB_ACC_REGISTER_EV_FAIL, command_mbreg_hook_cb, app);
    prot_main_push_tran(pmain, &(txnreq->htran));
    prot_main_push_tran(pmain, &(accreg->htran));

    prot_main_connect(pmain, argv[1], app->cf.mailbox_port, "127.0.0.1", app->cf.tor_port);
}

// Handle account remove status on pmain
static void command_mbrm_hook_cb(int ev, void *data, void *cbarg) {
    struct app_data *app = cbarg;

    if (ev != PROT_MB_ACC_DELETE_EV_OK) {
        app_ui_shell(app, "error: Failed to delete mailbox account");
        app_ui_shell(app, "You can delete account only locally by using mbrmlocal");
        return;
    }

    db_options_set_bin(app->db,  "client_mailbox_id",            NULL, 0);
    db_options_set_bin(app->db,  "client_mailbox_sig_pub_key",   NULL, 0);
    db_options_set_bin(app->db,  "client_mailbox_sig_priv_key",  NULL, 0);
    db_options_set_text(app->db, "client_mailbox_onion_address", NULL, 0);

    app_ui_shell(app, "Successfully deleted your mailbox account");
}

// Atempt to remove mailbox account
static void command_mbrm(int argc, char **argv, void *cbarg) {
    struct app_data *app = cbarg;

    struct prot_main *pmain;
    struct prot_mb_acc *accrm;
    struct prot_txn_req *txnreq;

    char mb_address[ONION_ADDRESS_LEN + 1];
    uint8_t mb_id[MAILBOX_ID_LEN];
    uint8_t mb_sig_priv_key[MAILBOX_ACCOUNT_KEY_PRIV_LEN];

    if (!db_options_is_defined(app->db, "client_mailbox_id", DB_OPTIONS_BIN)) {
        app_ui_shell(app, "error: You don't have mailbox account, nothing to delete");
        return;
    }

    app_ui_shell(app, "Attempting to delete your account from the mailbox server");
    app_ui_shell(app, "This could take some time...");

    db_options_get_bin(app->db, "client_mailbox_id", mb_id, MAILBOX_ID_LEN);
    db_options_get_bin(app->db, "client_mailbox_sig_priv_key", mb_sig_priv_key, MAILBOX_ACCOUNT_KEY_PRIV_LEN);
    db_options_get_text(app->db, "client_mailbox_onion_address", mb_address, ONION_ADDRESS_LEN + 1);

    pmain = prot_main_new(app->base, app->db);
    txnreq = prot_txn_req_new();
    accrm = prot_mb_acc_delete_new(app->db, mb_address, mb_id, mb_sig_priv_key);

    prot_main_free_on_done(pmain, 1);
    hook_add(pmain->hooks, PROT_MB_ACC_DELETE_EV_OK, command_mbrm_hook_cb, app);
    hook_add(pmain->hooks, PROT_MB_ACC_DELETE_EV_FAIL, command_mbrm_hook_cb, app);
    prot_main_push_tran(pmain, &(txnreq->htran));
    prot_main_push_tran(pmain, &(accrm->htran));

    prot_main_connect(pmain, mb_address, app->cf.mailbox_port, "127.0.0.1", app->cf.tor_port);
}

// Atempt to remove mailbox account
static void command_mbrmlocal(int argc, char **argv, void *cbarg) {
    struct app_data *app = cbarg;

    if (!db_options_is_defined(app->db, "client_mailbox_id", DB_OPTIONS_BIN)) {
        app_ui_shell(app, "error: You don't have mailbox account, nothing to delete");
        return;
    }

    app_ui_shell(app, "Deleting mailbox information from the database");
    app_ui_shell(app, "Your data still remains on the mailbox server");

    db_options_set_bin(app->db,  "client_mailbox_id",            NULL, 0);
    db_options_set_bin(app->db,  "client_mailbox_sig_pub_key",   NULL, 0);
    db_options_set_bin(app->db,  "client_mailbox_sig_priv_key",  NULL, 0);
    db_options_set_text(app->db, "client_mailbox_onion_address", NULL, 0);
}

static void command_friendadd_hook_cb(int ev, void *data, void *cbarg) {
    struct app_data *app = cbarg;
    struct db_contact *friend = data;

    if (ev != PROT_FRIEND_REQ_EV_OK) {
        app_ui_shell(app, "error: Failed to send the friend request");
        app_ui_shell(app, 
            "This can happen for various reasons but the most common one "
            "is that the person you are sending request to is offline, you "
            "must both be online in order to exchange encryption keys"
        );
        return;
    }

    if (friend->status == DB_CONTACT_ACTIVE) {
        app_ui_shell(app, "Successfully accepted %s's friend request you are now friends", 
            friend->nickname);
        app_update_contacts(app);
        ui_stack_redraw(app->ui.stack);
        return;
    }

    app_ui_shell(app,
        "Successfully sent friend request to the %s, when other "
        "client accepts your request you will be able to chat", friend->onion_address
    );
}

// Send friend request
static void command_friendadd(int argc, char **argv, void *cbarg) {
    struct app_data *app = cbarg;

    struct db_contact *friend;
    struct prot_main *pmain;
    struct prot_txn_req *txnreq;
    struct prot_friend_req *freq;

    if (!onion_address_valid(argv[1])) {
        app_ui_shell(app, "error: Invalid client onion address provided");
        return;
    }

    friend = db_contact_get_by_onion(app->db, argv[1], NULL);
    if (friend && friend->deleted) {
        app_ui_shell(app, "Removing deleted flag for given friend");
        friend->deleted = 0;
        db_contact_save(app->db, friend);

        if (friend->status == DB_CONTACT_ACTIVE) {
            app_update_contacts(app);
            ui_stack_redraw(app->ui.stack);
        }
    }
    if (friend && friend->status == DB_CONTACT_ACTIVE) {
        app_ui_shell(app, "You are already friend with [%s] %s", friend->nickname, argv[1]);
        app_ui_shell(app, "Nothing to do");
        return;
    }
    if (friend && friend->status == DB_CONTACT_PENDING_OUT) {
        app_ui_shell(app, "You have already sent friend request to this person");
        app_ui_shell(app, "Nothing to do");
        return;
    }

    app_ui_shell(app, "Sending friend request to %s", argv[1]);
    app_ui_shell(app, "This could take some time...");

    pmain = prot_main_new(app->base, app->db);
    txnreq = prot_txn_req_new();
    freq = prot_friend_req_new(app->db, argv[1]);

    prot_main_free_on_done(pmain, 1);
    hook_add(pmain->hooks, PROT_FRIEND_REQ_EV_OK, command_friendadd_hook_cb, app);
    hook_add(pmain->hooks, PROT_FRIEND_REQ_EV_FAIL, command_friendadd_hook_cb, app);
    prot_main_push_tran(pmain, &(txnreq->htran));
    prot_main_push_tran(pmain, &(freq->htran));

    prot_main_connect(pmain, argv[1], app->cf.app_port, "127.0.0.1", app->cf.tor_port);
}

// Send friend request
static void command_friends(int argc, char **argv, void *cbarg) {
    struct app_data *app = cbarg;
    int i, n_conts, non_del_cnt = 0;
    struct db_contact **conts;

    conts = db_contact_get_all(app->db, &n_conts);

    app_ui_shell(app, "List of all your friends and their statuses: ");

    for (i = 0; i < n_conts; i++) {
        if (conts[i]->deleted)
            continue;

        ++non_del_cnt;
        switch (conts[i]->status) {
            case DB_CONTACT_ACTIVE:
                app_ui_shell(app, "  - [%s] - %s - ACTIVE", 
                    conts[i]->nickname, conts[i]->onion_address);
                break;
            case DB_CONTACT_PENDING_IN:
                app_ui_shell(app, "  - [%s] - %s - INCOMMING PENDING", 
                    conts[i]->nickname, conts[i]->onion_address);
                break;
            case DB_CONTACT_PENDING_OUT:
                app_ui_shell(app, "  - [????] - %s - OUTGOING PENDING",
                    conts[i]->onion_address);
                break;
        }
    }

    app_ui_shell(app, "Total: %d", non_del_cnt);
}

// Delete given friend
static void command_friendrm(int argc, char **argv, void *cbarg) {
    struct app_data *app = cbarg;
    struct db_contact *cont;

    if (!onion_address_valid(argv[1])) {
        app_ui_shell(app, "error: Invalid client onion address provided");
        return;
    }

    cont = db_contact_get_by_onion(app->db, argv[1], NULL);
    if (!cont || cont->deleted) {
        app_ui_shell(app, "error: No such friend found in the database, nothing to delete");
        return;
    }

    cont->deleted = 1;
    db_contact_save(app->db, cont);
    app_ui_shell(app, "Marked friend %s as deleted", argv[1]);

    if (cont->status == DB_CONTACT_ACTIVE) {
        app_update_contacts(app);
        ui_stack_redraw(app->ui.stack);
    }
}

static void command_mbcontacts_hook_cb(int ev, void *data, void *cbarg) {
    struct app_data *app = cbarg;
    
    if (ev != PROT_MB_SET_CONTACTS_EV_OK) {
        app_ui_shell(app, "error: Failed to update contact list on the mailbox server");
        return;
    }

    app_ui_shell(app, "Successfully updated contact list on the mailbox server");
}

// Upload contact list to the mailbox server
static void command_mbcontacts(int argc, char **argv, void *cbarg) {
    struct app_data *app = cbarg;
    int n_conts;
    struct db_contact **conts;

    struct prot_main *pmain;
    struct prot_txn_req *txnreq;
    struct prot_mb_set_contacts *setconts;

    char mb_address[ONION_ADDRESS_LEN + 1];
    uint8_t mb_id[MAILBOX_ID_LEN];
    uint8_t mb_sig_priv_key[MAILBOX_ACCOUNT_KEY_PRIV_LEN];

    if (!db_options_is_defined(app->db, "client_mailbox_id", DB_OPTIONS_BIN)) {
        app_ui_shell(app, "error: You don't have mailbox account, cannot set contacts");
        return;
    }

    app_ui_shell(app, "Attempting to update contact list on the mailbox server");
    app_ui_shell(app, "This could take some time...");

    db_options_get_bin(app->db, "client_mailbox_id", mb_id, MAILBOX_ID_LEN);
    db_options_get_bin(app->db, "client_mailbox_sig_priv_key", mb_sig_priv_key, MAILBOX_ACCOUNT_KEY_PRIV_LEN);
    db_options_get_text(app->db, "client_mailbox_onion_address", mb_address, ONION_ADDRESS_LEN + 1);

    conts = db_contact_get_all(app->db, &n_conts);

    pmain = prot_main_new(app->base, app->db);
    txnreq = prot_txn_req_new();
    setconts = prot_mb_set_contacts_new(app->db, mb_address, mb_id, mb_sig_priv_key, conts, n_conts);

    prot_main_free_on_done(pmain, 1);
    hook_add(pmain->hooks, PROT_MB_SET_CONTACTS_EV_OK, command_mbcontacts_hook_cb, app);
    hook_add(pmain->hooks, PROT_MB_SET_CONTACTS_EV_FAIL, command_mbcontacts_hook_cb, app);
    prot_main_push_tran(pmain, &(txnreq->htran));
    prot_main_push_tran(pmain, &(setconts->htran));

    prot_main_connect(pmain, mb_address, app->cf.mailbox_port, "127.0.0.1", app->cf.tor_port);
}

// Upload sync messages with another client
static void command_sync(int argc, char **argv, void *cbarg) {
    struct app_data *app = cbarg;
    struct db_contact *cont;

    if (!onion_address_valid(argv[1])) {
        app_ui_shell(app, "error: Invalid client onion address provided");
        return;
    }

    if (!(cont = db_contact_get_by_onion(app->db, argv[1], NULL))) {
        app_ui_shell(app, "error: Cannot find given friend in the database");
        return;
    }

    app_ui_shell(app, "Attempting message sync in the background");
    app_contact_sync(app, cont);
}

// Set mb_direct configuration option value
static void command_mbdirect(int argc, char **argv, void *cbarg) {
    int value;
    struct app_data *app = cbarg;

    // Only valid values are 0 and 1
    if (sscanf(argv[1], "%d", &value) != 1 || (value != 1 && value != 0)) {
        app_ui_shell(app, "error: Only valid values for this option are 1 and 0");
        return;
    }

    app->cf.mb_direct = value;
    app_ui_shell(app, "Set app->cf.mb_direct = %d", value);
}

// Upload contact list to the mailbox server
static void command_mbsync(int argc, char **argv, void *cbarg) {
    struct app_data *app = cbarg;
    
    if (!db_options_is_defined(app->db, "client_mailbox_id", DB_OPTIONS_BIN)) {
        app_ui_shell(app, "error: Cannot sync with mailbox, you have no mailbox");
        return;
    }

    app_ui_shell(app, "Attempting message sync in the background");
    app_mailbox_sync(app);
}

// Upload contact list to the mailbox server
static void command_tor(int argc, char **argv, void *cbarg) {
    struct app_data *app = cbarg;
    
    if (app->tor_process) {
        app_ui_shell(app, "error: Tor has already started");
        return;
    }

    app_ui_shell(app, "Starting tor client");
    app_tor_start(app);
}

// Upload contact list
static void command_nickname(int argc, char **argv, void *cbarg) {
    int i;
    size_t len;
    struct app_data *app = cbarg;
    
    len = strlen(argv[1]);
    if (len < 4 || len >= CLIENT_NICK_MAX_LEN) {
        app_ui_shell(app, "error: Nickname length must be between 4 and %d characters",
            CLIENT_NICK_MAX_LEN - 1);
        return;
    }

    for (i = 0; i < app->n_contacts; i++) {
        struct db_message *msg;

        if (app->contacts[i]->deleted || app->contacts[i]->status != DB_CONTACT_ACTIVE)
            continue;

        msg = db_message_new();
        msg->type = DB_MESSAGE_NICK;
        msg->contact_id = app->contacts[i]->id;
        msg->sender = DB_MESSAGE_SENDER_ME;
        msg->status = DB_MESSAGE_STATUS_UNDELIVERED;
        db_message_gen_id(msg);

        msg->body_nick_len = len;
        strncpy(msg->body_nick, argv[1], CLIENT_NICK_MAX_LEN);
        db_message_save(app->db, msg);
        app_message_send(app, msg);
    }

    db_options_set_text(app->db, "client_nickname", argv[1], len);
    app_ui_shell(app, "Setting yout nickname to [%s]", argv[1]);
}

// Handle config shell commands
void app_ui_handle_cmd(struct ui_prompt *prt, void *att) {
    const char *err;
    struct app_data *app = att;

    // Command templates
    struct cmd_template cmds[] = {
        {"help",       0, command_help,       app},
        {"info",       0, command_info,       app},
        {"version",    0, command_version,    app},
        {"mbreg",      2, command_mbreg,      app},
        {"mbrm",       0, command_mbrm,       app},
        {"mbrmlocal",  0, command_mbrmlocal,  app},
        {"friendadd",  1, command_friendadd,  app},
        {"friends",    0, command_friends,    app},
        {"friendrm",   1, command_friendrm,   app},
        {"mbcontacts", 0, command_mbcontacts, app},
        {"sync",       1, command_sync,       app},
        {"mbdirect",   1, command_mbdirect,   app},
        {"mbsync",     0, command_mbsync,     app},
        {"tor",        0, command_tor,        app},
        {"nickname",   1, command_nickname,   app},
    };

    app_ui_shell(app, "> %ls", prt->input_buffer);

    if (err = cmd_parse(cmds, 15, ui_prompt_get_input(prt))) {
        app_ui_shell(app, "error: %s", err);
    }
    ui_prompt_clear(prt);
}