#ifndef _INCLUDE_UI_PFORM_H_
#define _INCLUDE_UI_PFORM_H_

/* Popup form */

#include <wchar.h>
#include <ui_window.h>

#define UI_PFORM_LABEL_SIZE 31

typedef void (*ui_pform_field_valid_cb)(
    struct ui_pform *pform,
    struct ui_pform_field *field,
    void *attr
);

enum ui_pform_field_type {
    UI_PFORM_TEXT, UI_PFORM_STR
};

struct ui_pform_field {
    ui_pform_field_type type;
    union {
        // Not an input field, just a chunk of text
        struct {
            int n_text;
            wchar_t *text;
        } type_text;
        // Normal text input field
        struct {
            int n_buffer;
            int max_len;
            wchar_t *buffer;
            int n_label;
            wchar_t label[UI_PFORM_LABEL_SIZE];
            ui_pform_field_valid_cb is_valid;
        } type_str;
    } data;
};

struct ui_pform {
    int n_fields;
    int fields_alloc;
    ui_pform_field *fields;
};

// Create new form component
struct ui_pform * ui_pform_new(int n_fields);

// Free form component memory
void ui_pform_free(struct ui_pform *pform);

// Add text (description) field to the form
void ui_pform_add_ftext(struct ui_pform *pform, wchar_t *text);

// Add string (text) input field to the form
void ui_pform_add_fstr(struct ui_pform *pform, wchar_t *label, ui_pform_field_valid_cb valid_cb);

#endif