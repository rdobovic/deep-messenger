#include <array.h>
#include <stdio.h>
#include <debug.h>
#include <string.h>
#include <stdlib.h>
#include <cmd_parse.h>

// Returns NULL on success or empty command, returns error string on error
const char * cmd_parse(const struct cmd_template *cmds, int cmdc, const char *cmdstr) {
    int i, argc;
    char *input, **argv, *res;
    static char error_str[CMD_MAX_ERR_LEN];

    res = NULL;
    argv = array(char*);
    input = array(char);
    array_strcpy(input, cmdstr, -1);

    if (array_set(argv, 0, strtok(input, " \n")) == NULL)
        goto end;
    
    for (i = 0; i < cmdc; i++) {
        if (strcmp(argv[0], cmds[i].name) == 0)
            break;
    }

    if (cmdc == i) {
        strcpy(error_str, "Unknown command");
        res = error_str;
        goto end;
    }

    for (argc = 1; array_set(argv, argc, strtok(NULL, " \n")) != NULL; argc++)
        /* Do nothing */;

    if (argc - 1 != cmds[i].arg_cnt) {
        snprintf(error_str, CMD_MAX_ERR_LEN, 
            "Wrong number of arguments for command %s, expected %d but got %d", cmds[i].name, cmds[i].arg_cnt, argc - 1);
        res = error_str;
        goto end;
    }

    // Call command callback
    cmds[i].cb(argc, argv, cmds[i].cbarg);

    end:
    array_free(argv);
    array_free(input);
    return res;
}