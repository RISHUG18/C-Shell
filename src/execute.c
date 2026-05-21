/*
 * execute.c — Command execution engine for C-Shell.
 *
 * Implements fork/exec dispatch for CommandGroups, Pipelines, and
 * individual Commands.  Uses execvp; handles ENOENT, EACCES, ENOEXEC.
 */

#include "../include/execute.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

/* ── apply_redirections ────────────────────────────────────────────────────
 * Called inside the child process after fork().
 * Returns 0 on success, -1 on failure (error already printed).
 */
static int apply_redirections(Command *cmd)
{
    if (cmd->input_file != NULL) {
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "No such file or directory: %s\n", cmd->input_file);
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2 stdin"); close(fd); return -1; }
        close(fd);
    }

    if (cmd->output_file != NULL) {
        int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
        int fd = open(cmd->output_file, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "Cannot open output file: %s\n", cmd->output_file);
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2 stdout"); close(fd); return -1; }
        close(fd);
    }

    return 0;
}

/* ── report_exec_error ─────────────────────────────────────────────────────
 * Called in the child when execvp fails.  Exits with the appropriate code.
 */
static void report_exec_error(const char *cmd_name)
{
    switch (errno) {
        case ENOENT:
        case ENOTDIR:
            fprintf(stderr, "Command not found!\n");
            _exit(127);
        case EACCES:
            fprintf(stderr, "Permission denied!\n");
            _exit(126);
        case ENOEXEC:
            fprintf(stderr, "Exec format error!\n");
            _exit(126);
        default:
            fprintf(stderr, "%s: %s\n", cmd_name ? cmd_name : "exec", strerror(errno));
            _exit(1);
    }
}

/* ── execute_single ────────────────────────────────────────────────────────
 * Fork + execvp for one simple Command (no pipeline context).
 */
void execute_single(Command *cmd, int *out_status)
{
    if (cmd == NULL || cmd->argv == NULL || cmd->argv[0] == NULL) return;

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        /* Child */
        if (apply_redirections(cmd) < 0) _exit(1);
        execvp(cmd->argv[0], cmd->argv);
        report_exec_error(cmd->argv[0]);
        /* report_exec_error calls _exit — never reached */
    }

    /* Parent */
    int status = 0;
    waitpid(pid, &status, 0);
    if (out_status != NULL) *out_status = status;
}

/* ── execute_pipeline ──────────────────────────────────────────────────────
 * Execute a Pipeline: fork one child per command, connect with pipes.
 */
void execute_pipeline(Pipeline *pl)
{
    if (pl == NULL || pl->head == NULL) return;

    /* Single-command pipeline: shortcut to execute_single */
    if (pl->cmd_count == 1) {
        execute_single(pl->head, NULL);
        return;
    }

    /* Multi-command pipeline */
    int n = pl->cmd_count;
    pid_t *pids = malloc(n * sizeof(pid_t));
    if (pids == NULL) { perror("malloc"); return; }

    int prev_read = -1;   /* read-end of the previous pipe */
    int cmd_idx   = 0;
    Command *cmd  = pl->head;

    while (cmd != NULL) {
        int pipefd[2] = {-1, -1};
        int is_last = (cmd->next == NULL);

        /* Create a pipe unless this is the last command */
        if (!is_last) {
            if (pipe(pipefd) < 0) { perror("pipe"); break; }
        }

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); break; }

        if (pid == 0) {
            /* Child: wire up stdin from previous pipe */
            if (prev_read != -1) {
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            /* Wire stdout to write-end of current pipe */
            if (!is_last) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[0]);
                close(pipefd[1]);
            }

            /* Per-command redirections (override pipe if specified) */
            if (apply_redirections(cmd) < 0) _exit(1);

            execvp(cmd->argv[0], cmd->argv);
            report_exec_error(cmd->argv[0]);
        }

        /* Parent: close ends we no longer need */
        if (prev_read != -1) close(prev_read);
        if (!is_last) {
            close(pipefd[1]);
            prev_read = pipefd[0];
        }

        pids[cmd_idx++] = pid;
        cmd = cmd->next;
    }

    /* Wait for all children */
    for (int i = 0; i < cmd_idx; i++) {
        waitpid(pids[i], NULL, 0);
    }

    free(pids);
}

/* ── execute_command_group_list ────────────────────────────────────────────
 * Top-level dispatcher: iterate over CommandGroups.
 * Foreground groups block the shell; background groups are forked off.
 */
void execute_command_group_list(CommandGroup *head)
{
    for (CommandGroup *cg = head; cg != NULL; cg = cg->next) {
        if (cg->is_background) {
            pid_t pid = fork();
            if (pid < 0) { perror("fork"); continue; }
            if (pid == 0) {
                /* Background child: run the pipeline and exit */
                execute_pipeline(cg->pipeline);
                exit(0);
            }
            /* Parent: don't wait — let it run in the background */
            printf("[bg] pid %d\n", pid);
        } else {
            execute_pipeline(cg->pipeline);
        }
    }
}
