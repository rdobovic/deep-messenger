#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <helpers.h>
#include <sys_process.h>


#define MAX_LINE 511

#define TOR_PATH "/bin/tor"

char *command[] = {"tor", "-f", "/home/roko/test/onion/torrc", NULL};

#define TOR_SUCCESS "Bootstrapped 100% (done): Done\n"

int main() {
    struct sys_process *proc;
    char line[MAX_LINE];

    proc = sys_process_start(TOR_PATH, command);

    printf("Started TOR process with PID %d\n", proc->pid);

    if (!proc) {
        printf("Failed to open the pipe and start the TOR process\n");
        exit(1);
    }

    while (fgets(line, MAX_LINE, proc->sout) != NULL) {
        int len = strlen(line);

        printf("%s", line);
        
        if (str_ends_with(line, TOR_SUCCESS)) {
            printf(">>>>> TOR is up and running <<<<<\n");
            break;
        }
    }

    sys_process_end(proc);
    printf(">>>>> End <<<<<\n");
}