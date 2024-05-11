#include <ncurses.h>
#include <ui_window.h>
#include <ui_manager.h>
#include <stdio.h>
#include <stdlib.h>
#include <debug.h>
#include <helpers.h>
#include <locale.h>
#include <wchar.h>
#include <ui_prompt.h>
#include <ui_stack.h>
#include <ui_logger.h>
#include <ui_menu.h>
#include <sys_memory.h>

#include <sqlite3.h>
#include <db_init.h>
#include <db_contact.h>
#include <db_message.h>

struct app_data {
    sqlite3 *db;

    int n_contacts;
    struct db_contact **contacts;
    struct db_contact *cont_selected;

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

void app_ui_define(struct app_data *app);
void app_ui_add_titles(struct app_data *app);

// Handle config shell commands
void app_ui_handle_cmd(struct ui_prompt *prt, void *att) {
    // ...
    ui_prompt_clear(prt);
}

// End win on crash
void app_ui_ncurses_crash(void *attr) {
    endwin();
}

void app_ui_go_shell(struct ui_menu *menu, void *att) {
    struct app_data *app = att;

    debug("SHELL");
    
    app->cont_selected = NULL;
    ui_prompt_attach(app->ui.prompt_cmd, app->ui.promptwin);
    ui_logger_attach(app->ui.shell, app->ui.chatwin);

    app_ui_define(app);
    app_ui_add_titles(app);
    ui_stack_redraw(app->ui.stack);
}

void app_ui_contact_select(struct ui_menu *menu, void *att) {
    struct app_data *app = att;
    int i;
    int n_messages;
    struct db_message **messages;
    wchar_t nick[CLIENT_NICK_MAX_LEN];

    debug("Selected %d", menu->cursor - 1);
    app->cont_selected = app->contacts[menu->cursor - 1];

    mbstowcs(nick, app->cont_selected->nickname, app->cont_selected->nickname_len + 1);
    debug("Nick OK");

    //ui_prompt_clear(app->ui.prompt_chat);
    //ui_prompt_attach(app->ui.prompt_chat, app->ui.promptwin);
    ui_logger_clear(app->ui.chat);
    ui_logger_attach(app->ui.chat, app->ui.chatwin);
    ui_logger_log(app->ui.chat, L"Start of chat");
    ui_logger_log(app->ui.chat, L" ");

    debug("CLR/ATT OK");
    

    messages = db_message_get_all(app->db, app->cont_selected, DB_MESSAGE_STATUS_ANY, &n_messages);

    debug("Got messages %d", n_messages);

    for (i = 0; i < n_messages; i++) {
        wchar_t *text;

        if (messages[i]->type != DB_MESSAGE_TEXT)
            continue;

        text = safe_malloc(sizeof(wchar_t) * messages[i]->body_text_len + 1,
            "Failed to print the msg");

        mbstowcs(text, messages[i]->body_text, messages[i]->body_text_len);
        text[messages[i]->body_text_len] = 0;

        ui_logger_printf(app->ui.chat, L"[%ls]: %ls", 
            messages[i]->sender == DB_MESSAGE_SENDER_ME ? L"ME" : nick, text);
        free(text);
    }
    
    app_ui_define(app);
    app_ui_add_titles(app);
    ui_stack_redraw(app->ui.stack);
}

void app_update_contacts(struct app_data *app) {
    int i;

    if (app->ui.contacts)
        ui_menu_free(app->ui.contacts);

    debug("herer");

    if (app->n_contacts > 0)
        db_contact_free_all(app->contacts, app->n_contacts);
    debug("herer");

    app->contacts = db_contact_get_all(app->db, &(app->n_contacts));

    debug("herer");

    app->ui.contacts = ui_menu_new();
    ui_menu_attach(app->ui.contacts, app->ui.contactswin);
    ui_menu_add(app->ui.contacts, 0, L"Config shell", app_ui_go_shell, app);

    debug("herer");

    for (i = 0; i < app->n_contacts; i++) {
        wchar_t nick[CLIENT_NICK_MAX_LEN];

        app->cont_selected = app->contacts[i];
        mbstowcs(nick, app->cont_selected->nickname, app->cont_selected->nickname_len + 1);
        ui_menu_add(app->ui.contacts, i + 1, nick, app_ui_contact_select, app);
    }
}

// Init all ui windows
void app_ui_init(struct app_data *app) {
    // Init ncurses
    initscr();
    curs_set(0);
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    start_color();
    use_default_colors();

    sys_crash_cb_add(app_ui_ncurses_crash, NULL);
    ui_window_start_colors();

    app->ui.stack = ui_stack_new();

    app->ui.mgrwin = ui_window_new();
    app->ui.chatwin = ui_window_new();
    app->ui.infowin = ui_window_new();
    app->ui.promptwin = ui_window_new();
    app->ui.contactswin = ui_window_new();

    app->ui.mgr = ui_manager_new(app->ui.mgrwin);

    debug("Allocated");

    ui_manager_add_window(app->ui.mgr, app->ui.infowin,     0);
    ui_manager_add_window(app->ui.mgr, app->ui.chatwin,     1);
    ui_manager_add_window(app->ui.mgr, app->ui.promptwin,   2);
    ui_manager_add_window(app->ui.mgr, app->ui.contactswin, 3);

    debug("Added");

    app->ui.prompt_cmd = ui_prompt_new();
    app->ui.prompt_chat = ui_prompt_new();

    ui_prompt_attach(app->ui.prompt_cmd, app->ui.promptwin);
    // set prompt submit...
    

    debug("prompt done");
    
    app->ui.info = ui_logger_new();
    app->ui.chat = ui_logger_new();
    app->ui.shell = ui_logger_new();
    ui_logger_attach(app->ui.info, app->ui.infowin);
    ui_logger_attach(app->ui.shell, app->ui.chatwin);

    debug("here");

    app_update_contacts(app);

    debug("Contacts OK");

    ui_stack_push(app->ui.stack, app->ui.mgrwin);
    refresh();
}

void app_ui_define(struct app_data *app) {
    int col1_w, col2_w, divider_w;
    int contacts_h, chat_h, prompt_h, info_h;

    ui_window_define(app->ui.mgrwin, 0, 0, 0, 0);

    divide("p100", app->ui.mgrwin->rows, &contacts_h);
    divide("p30p70f2", app->ui.mgrwin->rows, &info_h, &chat_h, &prompt_h);
    divide("p30f1p70", app->ui.mgrwin->cols, &col1_w, &divider_w, &col2_w);

    wmove(app->ui.mgrwin->content, 0, col1_w);
    wvline(app->ui.mgrwin->content, ACS_VLINE, app->ui.mgrwin->rows);

    ui_window_use_title(app->ui.chatwin, 1);
    ui_window_use_title(app->ui.infowin, 1);
    ui_window_use_title(app->ui.promptwin, 1);
    ui_window_use_title(app->ui.contactswin, 1);

    ui_window_define(app->ui.chatwin, info_h, col1_w + divider_w, chat_h, col2_w);
    ui_window_define(app->ui.infowin, 0, col1_w + divider_w, info_h, col2_w);
    ui_window_define(app->ui.promptwin, info_h + chat_h, col1_w + divider_w, prompt_h, col2_w);
    ui_window_define(app->ui.contactswin, 0, 0, contacts_h, col1_w);


}

void app_ui_add_titles(struct app_data *app) {
    wchar_t nick[CLIENT_NICK_MAX_LEN];

    debug("title for %p", app->cont_selected);

    if (app->cont_selected != NULL) {
        debug("Drawing cont title");
        mbstowcs(nick, app->cont_selected->nickname, app->cont_selected->nickname_len + 1);
        ui_window_write_title(app->ui.chatwin, nick, UI_WINDOW_LEFT);
        ui_window_write_title(app->ui.chatwin, L"CHAT", UI_WINDOW_CENTER);
        ui_window_write_title(app->ui.promptwin, L"Type your message", UI_WINDOW_LEFT);
    } else {
        debug("Drawing shell title");
        ui_window_write_title(app->ui.chatwin, L"CONFIG SHELL", UI_WINDOW_CENTER);
        ui_window_write_title(app->ui.promptwin, L"Type command", UI_WINDOW_LEFT);
    }
    
    ui_window_write_title(app->ui.infowin, L"Deep Messenger v1.0.0", UI_WINDOW_LEFT);
    ui_window_write_title(app->ui.infowin, L"INFO", UI_WINDOW_CENTER);
    ui_window_write_title(app->ui.contactswin, L"CONTACTS", UI_WINDOW_CENTER);
}

int main(void) {
    debug_init();
    setlocale(LC_ALL, "");
    
    struct app_data app;
    memset(&app, 0, sizeof(app));

    db_init_global("deep_messenger.db");
    db_init_schema(dbg);
    app.db = dbg;

    debug("STARTING");
    app_ui_init(&app);
    debug("INIT PASSED");
    app_ui_define(&app);
    debug("DEFINE PASSED");
    app_ui_add_titles(&app);
    debug("TITLES PASSED");

    ui_stack_redraw(app.ui.stack);
    debug("REDRAW PASSED");

    while (1) {
        int ch_type;
        wchar_t wch;

        ch_type = get_wch(&wch);

        if (wch == KEY_CTRL('X')) {
            break;
        }

        ui_stack_handle_input(app.ui.stack, wch, ch_type == KEY_CODE_YES);
    }
    endwin();
}