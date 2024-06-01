#include <ui_prompt.h>
#include <ui_logger.h>

#include <app.h>

// Handle config shell commands
void app_ui_handle_cmd(struct ui_prompt *prt, void *att) {
    struct app_data *app = att;
    // ...
    ui_logger_printf(app->ui.shell, "> %ls", prt->input_buffer);
    ui_prompt_clear(prt);
}

// Handle config shell commands
void app_ui_handle_msg(struct ui_prompt *prt, void *att) {
    struct app_data *app = att;
    // ...
    ui_logger_printf(app->ui.shell, "[message] %ls", prt->input_buffer);
    ui_prompt_clear(prt);
}