#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys_crash.h>
#include <debug.h>

static int n_crash_cbs = 0;
static sys_crash_cb crash_cbs[SYS_CRASH_MAX_CALLBACKS];
static void *crash_cb_arguments[SYS_CRASH_MAX_CALLBACKS];

// Crash the program with given source and error str
void sys_crash(const char *crash_source_str, const char *error_format, ...) {
    int i;
    va_list ap1, ap2;
    const char unk[] = "unknown";

    va_start(ap1, error_format);
    va_start(ap2, error_format);

    for (i = 0; i < n_crash_cbs; i++) {
        if (crash_cbs[i])
            crash_cbs[i](crash_cb_arguments[i]);
    }


    if (_debug_file) {
        fprintf(_debug_file, "\nCRASH: %s: ", crash_source_str ? crash_source_str : unk);
        vfprintf(_debug_file, (error_format ? error_format : unk), ap1);
        fprintf(_debug_file, "\n");
        fflush(_debug_file);
    }

    fprintf(stderr, "\n\nCRASH: %s: ", crash_source_str ? crash_source_str : unk);
    vfprintf(stderr, (error_format ? error_format : unk), ap2);
    fprintf(stderr, "\n\n");
    fflush(stderr);

    va_end(ap1);
    va_end(ap2);
    exit(1);
}

// Add new crash callback
void sys_crash_cb_add(sys_crash_cb cb, void *attr) {
    if (n_crash_cbs < SYS_CRASH_MAX_CALLBACKS) {
        crash_cbs[n_crash_cbs] = cb;
        crash_cb_arguments[n_crash_cbs++] = attr;
    }
}

// Get number of crash callbacks
int sys_crash_cb_count(void) {
    return n_crash_cbs;
}