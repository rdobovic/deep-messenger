#ifndef _INCLUDE_APP_H_
#define _INCLUDE_APP_H_

#include <sqlite3.h>
#include <event2/event.h>

#include <onion.h>
#include <constants.h>
#include <sys_process.h>
#include <helpers.h>
#include <ui_menu.h>
#include <ui_stack.h>
#include <ui_prompt.h>
#include <ui_logger.h>
#include <ui_window.h>
#include <ui_manager.h>
#include <prot_main.h>

// Log message to info UI window
#define app_ui_info(app, ...) \
    ui_logger_printf((app)->ui.info, __VA_ARGS__)

// Log message to shell/console UI window
#define app_ui_shell(app, ...) \
    ui_logger_printf((app)->ui.shell, __VA_ARGS__)

struct app_data {
    sqlite3 *db;

    // Libevent event base
    struct event_base *base;

    // Contacts array
    int n_contacts;
    struct db_contact **contacts;
    struct db_contact *cont_selected;

    // Some useful stuff
    char nickname[CLIENT_NICK_MAX_LEN + 1];
    char onion_address[ONION_ADDRESS_LEN + 1];

    // Set to 1 when tor client is ready
    int tor_ready;
    // Tor process object and stdout event
    struct event *tor_event;
    struct sys_process *tor_process;
    // Last line read from tor process
    char *tor_line_buffer;
    int tor_line_buffer_len;

    // Paths for all files needed by application
    struct {
        char *data_dir;
        char *torrc;
        char *db_file;
        char *onion_dir;
        char *tor_bin;
        char *tor_data;
    } path;

    // Global runtime config
    struct {
        int is_mailbox;
        // Client hidden service port
        char *app_port;
        // App will listen on this port locally
        char app_local_port[MAX_PORT_STR_LEN];
        // Mailbox hidden service port
        char *mailbox_port;
        // Tor SOCKS server port
        char tor_port[MAX_PORT_STR_LEN];
        // In manual mode some startup procedures are not ran
        int manual_mode;
    } cf;

    // Global UI related data
    struct {
        struct ui_stack *stack;

        struct ui_manager *mgr;
        struct ui_window *mgrwin;

        struct ui_window *chatwin;
        struct ui_logger *chat;
        struct ui_logger *shell;

        struct ui_window *contactswin;
        struct ui_menu *contacts;

        struct ui_window *promptwin;
        struct ui_prompt *prompt_cmd;
        struct ui_prompt *prompt_chat;

        struct ui_window *infowin;
        struct ui_logger *info;
    } ui;
};

// Event priorities higher to lower
enum app_event_priorities {
    APP_EV_PRIORITY_PRIMARY, // Any more important action
    APP_EV_PRIORITY_USER,    // User interactions (reading from stdin)
    APP_EV_PRIORITY_NET,     // Handling network traffic
    APP_EV_PRIORITY_COUNT,   // Number of event priorities
};

// Start application with given cmd arguments
void app_start(struct app_data *app, int argc, char **argv);
// Exit application (do needed exit procedures)
void app_end(struct app_data *app);

// Init libevent and standard input event
void app_event_init(struct app_data *app);
// Start event loop
void app_event_start(struct app_data *app);
// Stop event loop
void app_event_end(struct app_data *app);

// Generate torrc, start tor client,
// also add input event to event loop to handle input from client
void app_tor_start(struct app_data *app);
// Stop tor process and associated event
void app_tor_end(struct app_data *app);

// Init ncurses and all ui windows and components
void app_ui_init(struct app_data *app);
// End ncurses
void app_ui_end(struct app_data *app);
// Calculate window dimensions and assign them to windows
void app_ui_define(struct app_data *app);
// Add titles to all windows
void app_ui_add_titles(struct app_data *app);

// Prompt callback to handle command submit
void app_ui_handle_cmd(struct ui_prompt *prt, void *att);
// Prompt callback to handle new message submit
void app_ui_handle_msg(struct ui_prompt *prt, void *att);

// Menu callback used to open messenger console
void app_ui_shell_select(struct ui_menu *menu, void *att);
// Menu callback used to select contact
void app_ui_contact_select(struct ui_menu *menu, void *att);

// Refresh chat window with new messages
void app_ui_chat_refresh(struct app_data *app, int keep_position);

// Refresh displayed contacts list
void app_update_contacts(struct app_data *app);

// Add hooks to main protocol handler for incomming connection (app_actions.c)
void app_pmain_add_hooks(struct app_data *app, struct prot_main *pmain);

#endif