#ifndef _SYS_PROCESS_
#define _SYS_PROCESS_

#include <stdio.h>
#include <unistd.h>

#define SYS_PROCESS_READ   0
#define SYS_PROCESS_WRITE  1

#define SYS_PROCESS_MEMORY_ERROR "Failed to allocate memory for process data structure"

struct sys_process {
    pid_t pid;

    int stdin_fd;
    int stdout_fd;

    FILE *sin;
    FILE *sout;
};

// Start program on given path with given arguments (NULL terminated)
// and return process data (pipes and pid)
struct sys_process * sys_process_start(const char *filepath, char **args);

// Kill process and close pipes
void sys_process_end(struct sys_process *process);


#endif