#include <ui_prompt.h>
#include <ui_logger.h>
#include <string.h>
#include <array.h>
#include <cmd_parse.h>
#include <constants.h>
#include <onion.h>
#include <db_options.h>
#include <debug.h>

#include <app.h>

// Print help message to the console
static void command_help(int argc, char **argv, void *cbarg) {
    struct app_data *app = cbarg;

    ui_logger_log(app->ui.shell, "List of all available commands:");
    ui_logger_log(app->ui.shell, "  help                    Prints this help message");
    ui_logger_log(app->ui.shell, "  mailboxfetch            Fetch new messages from mailbox server");
    ui_logger_log(app->ui.shell, "  fetch <onion>           Fetch new messages from given client");
    ui_logger_log(app->ui.shell, "  friends                 List all friends");
    ui_logger_log(app->ui.shell, "  friendadd <onion>       Send friend request to add a new friend");
    ui_logger_log(app->ui.shell, "  friendrm <onion>        Remove given friend");
    ui_logger_log(app->ui.shell, "  info                    Print your account info");
    ui_logger_log(app->ui.shell, "  nickname <nick>         Change your nickname to <nick>");
    ui_logger_log(app->ui.shell, "  mailboxregister <onion> Register to given mailbox server");
    ui_logger_log(app->ui.shell, "  mailboxrm               Delete account on your current mailbox server");
    ui_logger_log(app->ui.shell, "  version                 Prints app and protocol version");
}

// Print application version to the console
static void command_version(int argc, char **argv, void *cbarg) {
    struct app_data *app = cbarg;

    ui_logger_printf(app->ui.shell, "Deep Messenger version %s (protocol v%d)",
        DEEP_MESSENGER_APP_VERSION, DEEP_MESSENGER_PROTOCOL_VER);
    ui_logger_printf(app->ui.shell, "Made by rdobovic");
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

    ui_logger_printf(app->ui.shell, "Your account information:");
    ui_logger_printf(app->ui.shell, "  nickname:              %s", nick);
    ui_logger_printf(app->ui.shell, "  your onion address:    %s", onion_address);
    ui_logger_printf(app->ui.shell, "  mailbox onion address: %s", 
        has_mailbox ? mb_onion_address : "(you have no mailbox)");
}

// Handle config shell commands
void app_ui_handle_cmd(struct ui_prompt *prt, void *att) {
    const char *err;
    struct app_data *app = att;

    // Command templates
    struct cmd_template cmds[] = {
        {"help",    0, command_help,    app},
        {"info",    0, command_info,    app},
        {"version", 0, command_version, app},
    };

    ui_logger_printf(app->ui.shell, "> %ls", prt->input_buffer);

    if (err = cmd_parse(cmds, 3, ui_prompt_get_input(prt))) {
        ui_logger_printf(app->ui.shell, "error: %s", err);
    }
    ui_logger_log(app->ui.shell, "");
    ui_prompt_clear(prt);
}

// Handle config shell commands
void app_ui_handle_msg(struct ui_prompt *prt, void *att) {
    struct app_data *app = att;
    // ...
    ui_logger_printf(app->ui.shell, "[message] %ls", prt->input_buffer);
    ui_prompt_clear(prt);
}