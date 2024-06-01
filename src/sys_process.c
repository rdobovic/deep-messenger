#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <debug.h>

#include <sys_memory.h>
#include <sys_process.h>

struct sys_process * sys_process_start(const char *filepath, char **args, int open_files) {
    struct sys_process *proc;

    pid_t pid;
    int p_stdin[2], p_stdout[2];

    if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
        return NULL;

    pid = fork();

    if (pid < 0)
        return NULL;

    else if (pid == 0) {
        close(p_stdin[SYS_PROCESS_WRITE]);
        close(p_stdout[SYS_PROCESS_READ]);

        dup2(p_stdin[SYS_PROCESS_READ], SYS_PROCESS_READ);
        dup2(p_stdout[SYS_PROCESS_WRITE], SYS_PROCESS_WRITE);

        execv(filepath, args);
        perror("execv");
        exit(1);
    }

    proc = safe_malloc(sizeof(struct sys_process), SYS_PROCESS_MEMORY_ERROR);
    memset(proc, 0, sizeof(struct sys_process));

    proc->pid = pid;

    proc->stdin_fd = p_stdin[SYS_PROCESS_WRITE];
    proc->stdout_fd = p_stdout[SYS_PROCESS_READ];

    if (open_files) {
        proc->sin = fdopen(proc->stdin_fd, "w");
        proc->sout = fdopen(proc->stdout_fd, "r");
    }

    return proc;
}

void sys_process_end(struct sys_process *process) {
    if (process->sin)
        fclose(process->sin);
    if (process->sout)
        fclose(process->sout);
    
    close(process->stdin_fd);
    close(process->stdout_fd);

    kill(process->pid, SIGTERM);
    free(process);
}
