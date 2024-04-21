#ifndef _INCLUDE_SYS_CRASH_H_
#define _INCLUDE_SYS_CRASH_H_

#define SYS_CRASH_MAX_CALLBACKS 16

// Callback type to call on crash
typedef void (*sys_crash_cb)(void *attr);

// Crash application with given error
void sys_crash(const char *crash_source_str, const char *error_format, ...);

// Add callback to call on memory error
void sys_crash_cb_add(sys_crash_cb cb, void *attr);

// Get number of crash callbacks defined
int sys_crash_cb_count(void);

#endif