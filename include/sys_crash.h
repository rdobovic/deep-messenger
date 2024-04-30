#ifndef _INCLUDE_SYS_CRASH_H_
#define _INCLUDE_SYS_CRASH_H_

#define SYS_CRASH_MAX_CALLBACKS 16

#include <openssl/err.h>
// This should be moved somewhere else in the future
#define sys_openssl_crash(desc) \
    sys_crash("Openssl", "%s, with error: %s", (desc), ERR_error_string(ERR_get_error(), NULL))

// Callback type to call on crash
typedef void (*sys_crash_cb)(void *attr);

// Crash application with given error
void sys_crash(const char *crash_source_str, const char *error_format, ...);

// Add callback to call on memory error
void sys_crash_cb_add(sys_crash_cb cb, void *attr);

// Get number of crash callbacks defined
int sys_crash_cb_count(void);

#endif