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
#include <prot_transaction.h>
#include <prot_mb_account.h>

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

// Handle config shell commands
void app_ui_handle_cmd(struct ui_prompt *prt, void *att) {
    const char *err;
    struct app_data *app = att;

    // Command templates
    struct cmd_template cmds[] = {
        {"help",      0, command_help,      app},
        {"info",      0, command_info,      app},
        {"version",   0, command_version,   app},
        {"mbreg",     2, command_mbreg,     app},
        {"mbrm",      0, command_mbrm,      app},
        {"mbrmlocal", 0, command_mbrmlocal, app},
    };

    app_ui_shell(app, "\n> %ls", prt->input_buffer);

    if (err = cmd_parse(cmds, 6, ui_prompt_get_input(prt))) {
        app_ui_shell(app, "error: %s", err);
    }
    ui_prompt_clear(prt);
}