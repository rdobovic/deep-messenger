#include <debug.h>
#include <stdio.h>
#include <cmd_parse.h>

void command_name(int argc, char **argv, void *cbarg) {
    debug("Hi %s", argv[1]);
}

void command_add(int argc, char **argv, void *cbarg) {
    int a, b;

    if (sscanf(argv[1], "%d", &a) != 1 || sscanf(argv[2], "%d", &b) != 1) {
        debug("error: Arguments must be 2 integers");
    }

    debug("%d + %d = %d", a, b, a + b);
}

void command_version(int argc, char **argv, void *cbarg) {
    debug("I don't know my version :)");
}

int main() {
    char buffer[255];
    struct cmd_template cmds[] = {
        {"name",    1, command_name,    NULL},
        {"add",     2, command_add,     NULL},
        {"version", 0, command_version, NULL},
    };

    debug_set_fp(stdout);

    while (1) {
        const char *err;
        printf("> ");
        fgets(buffer, sizeof(buffer), stdin);

        if (err = cmd_parse(cmds, 3, buffer))
            debug("error: %s", err);
    }

    return 0;
}