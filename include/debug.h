#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdio.h>
#include <string.h>

/* This should be switched using make file later */
#define DEEP_MESSENGER_DEBUG

#define DEBUG_LOG_FILE "debug.log"

extern FILE *_debug_file;

#ifdef DEEP_MESSENGER_DEBUG

    #define debug(...) \
        if (_debug_file) { \
            fprintf(_debug_file, "line [%04d] in %-30s ", __LINE__, "["__FILE__"]"); \
            fprintf(_debug_file, __VA_ARGS__); \
            fprintf(_debug_file, "\n"); \
            fflush(_debug_file); \
        }

    #define vdebug(format, ap) \
        if (_debug_file) { \
            fprintf(_debug_file, "line [%04d] in %-30s ", __LINE__, "["__FILE__"]"); \
            vfprintf(_debug_file, (format), (ap)); \
            fprintf(_debug_file, "\n"); \
            fflush(_debug_file); \
        }

    #define debug_init() \
        if (1) { \
            _debug_file = fopen(DEBUG_LOG_FILE, "w"); \
            debug(">> Starting debugger <<"); \
        }

    #define debug_set_fp(fp) \
        if (1) { \
            _debug_file = (fp); \
            debug(">> New debug FP set"); \
        }

#else

    #define debug(...)
    #define vdebug(format, ap)
    #define debug_init()
    #define debug_set_fp(fp)

#endif
#endif