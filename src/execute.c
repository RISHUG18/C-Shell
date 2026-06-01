#include "../include/execute.h"
#include "../include/builtins/hop.h"
#include "../include/builtins/reveal.h"
#include "../include/builtins/log.h"
#include "../include/builtins/activities.h"
#include "../include/builtins/ping.h"
#include "../include/jobs.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>

static int shell_tty = -1;

void init_execute(void)
{
    shell_tty = open("/dev/tty", O_RDWR);
    if (shell_tty < 0) shell_tty = STDIN_FILENO;
    setpgid(0, 0);
    tcsetpgrp(shell_tty, getpgid(0));
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
}

static int apply_redirections(Command *cmd)
{
    if (cmd->input_file) {
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) { fprintf(stderr, "No such file or directory\n"); return -1; }
        if (dup2(fd, STDIN_FILENO) < 0) { close(fd); return -1; }
        close(fd);
    }
    if (cmd->output_file) {
        int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
        int fd = open(cmd->output_file, flags, 0644);
        if (fd < 0) { fprintf(stderr, "Unable to create file for writing\n"); return -1; }
        if (dup2(fd, STDOUT_FILENO) < 0) { close(fd); return -1; }
        close(fd);
    }
    return 0;
}

static void exec_error(const char *name)
{
    switch (errno) {
        case ENOENT: case ENOTDIR:
            fprintf(stderr, "Command not found!\n"); _exit(127);
        case EACCES:
            fprintf(stderr, "Permission denied!\n"); _exit(126);
        case ENOEXEC:
            fprintf(stderr, "Exec format error!\n"); _exit(126);
        default:
            fprintf(stderr, "%s: %s\n", name ? name : "exec", strerror(errno));
            _exit(1);
    }
}

void execute_single(Command *cmd, int *out_status)
{
    if (!cmd || !cmd->argv || !cmd->argv[0]) return;

    if (strcmp(cmd->argv[0], "hop")        == 0) { builtin_hop(cmd);        return; }
    if (strcmp(cmd->argv[0], "reveal")     == 0) { builtin_reveal(cmd);     return; }
    if (strcmp(cmd->argv[0], "log")        == 0) { builtin_log(cmd);        return; }
    if (strcmp(cmd->argv[0], "activities") == 0) { builtin_activities(cmd); return; }
    if (strcmp(cmd->argv[0], "ping")       == 0) { builtin_ping(cmd);       return; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        setpgid(0, 0);
        if (apply_redirections(cmd) < 0) _exit(1);
        execvp(cmd->argv[0], cmd->argv);
        exec_error(cmd->argv[0]);
    }

    setpgid(pid, pid);
    fg_pgid = pid;
    if (shell_tty >= 0) tcsetpgrp(shell_tty, pid);

    int st = 0;
    waitpid(pid, &st, WUNTRACED);

    if (shell_tty >= 0) tcsetpgrp(shell_tty, getpgid(0));
    fg_pgid = 0;

    if (WIFSTOPPED(st)) {
        const char *name = cmd->argv[0];
        int idx = add_job(pid, name);
        jobs[idx].state = JOB_STOPPED;
        printf("\n[%d] Stopped   %s\n", jobs[idx].job_num, name);
    } else {
        if (out_status) *out_status = st;
    }
}

void execute_pipeline(Pipeline *pl)
{
    if (!pl || !pl->head) return;

    if (pl->cmd_count == 1) {
        execute_single(pl->head, NULL);
        return;
    }

    pid_t pgid = 0;
    pid_t *pids = malloc(pl->cmd_count * sizeof(pid_t));
    if (!pids) { perror("malloc"); return; }

    int prev_fd = -1, spawned = 0;
    Command *cmd = pl->head;

    while (cmd) {
        int pfd[2] = {-1, -1};
        int last = (cmd->next == NULL);

        if (!last && pipe(pfd) < 0) {
            perror("pipe");
            if (prev_fd != -1) close(prev_fd);
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            if (!last) { close(pfd[0]); close(pfd[1]); }
            if (prev_fd != -1) close(prev_fd);
            break;
        }

        if (pid == 0) {
            if (pgid == 0) pgid = getpid();
            setpgid(0, pgid);
            if (prev_fd != -1) { dup2(prev_fd, STDIN_FILENO); close(prev_fd); }
            if (!last) { dup2(pfd[1], STDOUT_FILENO); close(pfd[0]); close(pfd[1]); }
            if (apply_redirections(cmd) < 0) _exit(1);
            execvp(cmd->argv[0], cmd->argv);
            exec_error(cmd->argv[0]);
        }

        if (pgid == 0) pgid = pid;
        setpgid(pid, pgid);

        if (prev_fd != -1) close(prev_fd);
        if (!last) { close(pfd[1]); prev_fd = pfd[0]; }
        else        prev_fd = -1;

        pids[spawned++] = pid;
        cmd = cmd->next;
    }

    fg_pgid = pgid;
    if (shell_tty >= 0 && pgid > 0) tcsetpgrp(shell_tty, pgid);

    for (int i = 0; i < spawned; i++) waitpid(pids[i], NULL, WUNTRACED);

    if (shell_tty >= 0) tcsetpgrp(shell_tty, getpgid(0));
    fg_pgid = 0;

    if (prev_fd != -1) close(prev_fd);
    free(pids);
}

void execute_command_group_list(CommandGroup *head)
{
    for (CommandGroup *cg = head; cg; cg = cg->next) {
        if (cg->is_background) {
            pid_t pid = fork();
            if (pid < 0) { perror("fork"); continue; }
            if (pid == 0) {
                setpgid(0, 0);
                int devnull = open("/dev/null", O_RDONLY);
                if (devnull >= 0) { dup2(devnull, STDIN_FILENO); close(devnull); }
                execute_pipeline(cg->pipeline);
                exit(0);
            }
            setpgid(pid, pid);
            const char *name = (cg->pipeline && cg->pipeline->head &&
                                cg->pipeline->head->argv &&
                                cg->pipeline->head->argv[0])
                               ? cg->pipeline->head->argv[0] : "?";
            int idx = add_job(pid, name);
            printf("[%d] %d\n", jobs[idx].job_num, pid);
        } else {
            execute_pipeline(cg->pipeline);
        }
    }
}
