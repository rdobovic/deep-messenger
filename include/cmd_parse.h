#ifndef _INCLUDE_CMD_PARSE_H_
#define _INCLUDE_CMD_PARSE_H_

#define CMD_MAX_ERR_LEN 120

// First argument of argv is command name
typedef void (*cmd_cb)(int argc, char **argv, void *cbarg);

// Array of this structures must be passed as command templates
struct cmd_template {
    char *name;
    int arg_cnt;
    cmd_cb cb;
    void *cbarg;
};

// Returns NULL on success or empty command, returns error string on error
const char * cmd_parse(const struct cmd_template *cmds, int cmdc, const char *cmdstr);

#endif
