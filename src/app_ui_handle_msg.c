#include <ui_prompt.h>
#include <db_contact.h>
#include <app.h>

// Handle config shell commands
void app_ui_handle_msg(struct ui_prompt *prt, void *att) {
    struct app_data *app = att;
    // ...
    app_ui_info(app, "Sending new message to [%s] %s", app->cont_selected->nickname, ui_prompt_get_input(prt));
    ui_prompt_clear(prt);
}