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

#include <app.h>

// End win on crash
static void app_ui_ncurses_crash(void *attr) {
    app_ui_end(attr);
}

void app_ui_shell_select(struct ui_menu *menu, void *att) {
    struct app_data *app = att;
    
    app->cont_selected = NULL;
    ui_prompt_attach(app->ui.prompt_cmd, app->ui.promptwin);
    ui_logger_attach(app->ui.shell, app->ui.chatwin);

    app_ui_add_titles(app);
    ui_stack_redraw(app->ui.stack);
}

void app_ui_chat_refresh(struct app_data *app, int keep_position) {
    int i, i_line, i_wrap;
    int n_messages;
    struct db_message **messages;

    if (!app->cont_selected)
        return;

    i_line = app->ui.chat->i_line;
    i_wrap = app->ui.chat->i_wrap;

    ui_logger_clear(app->ui.chat);
    ui_logger_printf(app->ui.chat, "== Start of chat with [%s] == %s ==\n",
        app->cont_selected->nickname, app->cont_selected->onion_address);

    messages = db_message_get_all(app->db, app->cont_selected, DB_MESSAGE_STATUS_ANY, &n_messages);

    for (i = 0; i < n_messages; i++) {
        wchar_t *text;
        char status;

        if (messages[i]->type != DB_MESSAGE_TEXT)
            continue;

        switch (messages[i]->status) {
            case DB_MESSAGE_STATUS_RECV:           status = 'r'; break;
            case DB_MESSAGE_STATUS_RECV_CONFIRMED: status = 'R'; break;
            case DB_MESSAGE_STATUS_SENT:           status = 's'; break;
            case DB_MESSAGE_STATUS_SENT_CONFIRMED: status = 'S'; break;
            case DB_MESSAGE_STATUS_UNDELIVERED:    status = 'U'; break;
        }

        if (messages[i]->sender == DB_MESSAGE_SENDER_ME) {
            ui_logger_printf(app->ui.chat, "%*s[me] |%c| %s",
                strlen(app->cont_selected->nickname) - 2, "", status, messages[i]->body_text);
        } else {
            ui_logger_printf(app->ui.chat, "[%s] |%c| %s",
                app->cont_selected->nickname, status, messages[i]->body_text);
        }
    }

    if (keep_position) {
        app->ui.chat->i_line = i_line;
        app->ui.chat->i_wrap = i_wrap;
    }

    app_ui_add_titles(app);
    ui_stack_redraw(app->ui.stack);
}

void app_ui_contact_select(struct ui_menu *menu, void *att) {
    struct app_data *app = att;
    int i;
    int n_messages;
    struct db_message **messages;

    app->cont_selected = app->contacts[menu->cursor - 1];

    ui_prompt_clear(app->ui.prompt_chat);
    ui_prompt_attach(app->ui.prompt_chat, app->ui.promptwin);

    ui_logger_clear(app->ui.chat);
    ui_logger_attach(app->ui.chat, app->ui.chatwin);

    app_ui_chat_refresh(app, 0);
}

void app_update_contacts(struct app_data *app) {
    int i;

    if (app->ui.contacts)
        ui_menu_free(app->ui.contacts);

    if (app->n_contacts > 0)
        db_contact_free_all(app->contacts, app->n_contacts);

    app->contacts = db_contact_get_all(app->db, &(app->n_contacts));

    app->ui.contacts = ui_menu_new();
    ui_menu_attach(app->ui.contacts, app->ui.contactswin);
    ui_menu_add(app->ui.contacts, 0, "console", app_ui_shell_select, app);

    for (i = 0; i < app->n_contacts; i++) {
        char label[UI_MENU_LABEL_SIZE];
        app->cont_selected = app->contacts[i];

        if (app->cont_selected->deleted || app->cont_selected->status != DB_CONTACT_ACTIVE)
            continue;

        snprintf(label, UI_MENU_LABEL_SIZE - 1, "@%s", app->cont_selected->nickname);
        ui_menu_add(app->ui.contacts, i + 1, label, app_ui_contact_select, app);
    }

    app->cont_selected = NULL;
}

// Init all ui windows
void app_ui_init(struct app_data *app) {
    initscr();
    curs_set(0);
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    start_color();
    use_default_colors();
    timeout(0);

    setlocale(LC_ALL, "");

    sys_crash_cb_add(app_ui_ncurses_crash, app);
    ui_window_start_colors();

    app->ui.stack = ui_stack_new();

    app->ui.mgrwin = ui_window_new();
    app->ui.chatwin = ui_window_new();
    app->ui.infowin = ui_window_new();
    app->ui.promptwin = ui_window_new();
    app->ui.contactswin = ui_window_new();

    app->ui.mgr = ui_manager_new(app->ui.mgrwin);

    ui_manager_add_window(app->ui.mgr, app->ui.infowin,     0);
    ui_manager_add_window(app->ui.mgr, app->ui.chatwin,     1);
    ui_manager_add_window(app->ui.mgr, app->ui.promptwin,   2);
    ui_manager_add_window(app->ui.mgr, app->ui.contactswin, 3);

    app->ui.prompt_cmd = ui_prompt_new();
    app->ui.prompt_chat = ui_prompt_new();

    ui_prompt_attach(app->ui.prompt_cmd, app->ui.promptwin);
    ui_prompt_set_submit_cb(app->ui.prompt_cmd, app_ui_handle_cmd, app);
    ui_prompt_set_submit_cb(app->ui.prompt_chat, app_ui_handle_msg, app);
    
    app->ui.info = ui_logger_new();
    app->ui.chat = ui_logger_new();
    app->ui.shell = ui_logger_new();
    ui_logger_attach(app->ui.info, app->ui.infowin);
    ui_logger_attach(app->ui.shell, app->ui.chatwin);

    app_update_contacts(app);

    ui_stack_push(app->ui.stack, app->ui.mgrwin);
    refresh();
}

void app_ui_end(struct app_data *app) {
    endwin();
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

    ui_window_clear_title(app->ui.chatwin);
    ui_window_clear_title(app->ui.promptwin);

    if (app->cont_selected != NULL) {
        ui_window_write_title(app->ui.chatwin, app->cont_selected->nickname, UI_WINDOW_LEFT);
        ui_window_write_title(app->ui.chatwin, "CHAT", UI_WINDOW_CENTER);
        ui_window_write_title(app->ui.promptwin, "Type your message", UI_WINDOW_LEFT);
    } else {
        ui_window_write_title(app->ui.chatwin, "CONSOLE", UI_WINDOW_CENTER);
        ui_window_write_title(app->ui.promptwin, "Type command", UI_WINDOW_LEFT);
    }
    
    ui_window_clear_title(app->ui.infowin);
    ui_window_write_title(app->ui.infowin, "INFO", UI_WINDOW_CENTER);
    ui_window_clear_title(app->ui.contactswin);
    ui_window_write_title(app->ui.contactswin, "Deep Messenger", UI_WINDOW_LEFT);
}