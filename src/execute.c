/*
 * execute.c — Command execution engine for C-Shell.
 *
 * Implements fork/exec dispatch for CommandGroups, Pipelines, and
 * individual Commands.  Uses execvp; handles ENOENT, EACCES, ENOEXEC.
 * Built-in commands (hop, etc.) are intercepted before forking.
 */

#include "../include/execute.h"
#include "../include/builtins/hop.h"
#include "../include/builtins/reveal.h"
#include "../include/builtins/log.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

/* ── apply_redirections ────────────────────────────────────────────────────
 * Called inside the child process after fork().
 * Applies input (<) and output (> / >>) redirections using open()+dup2().
 * Exact error strings match the evaluation spec.
 * Returns 0 on success, -1 on failure (error already printed to stderr).
 */
static int apply_redirections(Command *cmd)
{
    if (cmd->input_file != NULL) {
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "No such file or directory\n");
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            fprintf(stderr, "dup2: stdin redirect failed\n");
            close(fd); return -1;
        }
        close(fd);
    }

    if (cmd->output_file != NULL) {
        int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
        int fd = open(cmd->output_file, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, "Unable to create file for writing\n");
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            fprintf(stderr, "dup2: stdout redirect failed\n");
            close(fd); return -1;
        }
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

    /* ── Built-ins: run in parent process ── */
    if (strcmp(cmd->argv[0], "hop") == 0) {
        builtin_hop(cmd);
        return;
    }
    if (strcmp(cmd->argv[0], "reveal") == 0) {
        builtin_reveal(cmd);
        return;
    }
    if (strcmp(cmd->argv[0], "log") == 0) {
        builtin_log(cmd);
        return;
    }

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
 * Execute a Pipeline by forking one child per command.
 *
 * Algorithm:
 *   - Maintain prev_fd = read-end of the previous pipe (-1 for first cmd).
 *   - For each command (except last), create a pipe().
 *   - In child: dup2(prev_fd → stdin), dup2(write-end → stdout), then
 *     apply file redirections (these override the pipe ends if specified).
 *   - In parent: close prev_fd and the write-end of the new pipe; save
 *     the read-end as the new prev_fd.
 *   - After all forks, waitpid() on EVERY child before returning.
 */
void execute_pipeline(Pipeline *pl)
{
    if (pl == NULL || pl->head == NULL) return;

    /* Single-command shortcut */
    if (pl->cmd_count == 1) {
        execute_single(pl->head, NULL);
        return;
    }

    int n = pl->cmd_count;
    pid_t *pids = malloc(n * sizeof(pid_t));
    if (pids == NULL) { perror("malloc"); return; }

    int prev_fd  = -1;   /* read-end of the pipe connecting previous → current */
    int spawned  = 0;
    Command *cmd = pl->head;

    while (cmd != NULL) {
        int pfd[2] = {-1, -1};
        int is_last = (cmd->next == NULL);

        /* Open the next pipe unless this is the last stage */
        if (!is_last && pipe(pfd) < 0) {
            perror("pipe");
            if (prev_fd != -1) close(prev_fd);
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            if (!is_last) { close(pfd[0]); close(pfd[1]); }
            if (prev_fd != -1) close(prev_fd);
            break;
        }

        if (pid == 0) {
            /* ── Child ── */

            /* Connect stdin to the read-end of the previous pipe */
            if (prev_fd != -1) {
                if (dup2(prev_fd, STDIN_FILENO) < 0) _exit(1);
                close(prev_fd);
            }

            /* Connect stdout to the write-end of the new pipe */
            if (!is_last) {
                if (dup2(pfd[1], STDOUT_FILENO) < 0) _exit(1);
                close(pfd[0]);
                close(pfd[1]);
            }

            /* File redirections override pipe endpoints if present */
            if (apply_redirections(cmd) < 0) _exit(1);

            execvp(cmd->argv[0], cmd->argv);
            report_exec_error(cmd->argv[0]);
            /* never reached */
        }

        /* ── Parent ── */

        /* We no longer need the read-end of the previous pipe */
        if (prev_fd != -1) close(prev_fd);

        /* Close the write-end of the new pipe (child owns it now) */
        if (!is_last) {
            close(pfd[1]);
            prev_fd = pfd[0];   /* save read-end for the next iteration */
        } else {
            prev_fd = -1;
        }

        pids[spawned++] = pid;
        cmd = cmd->next;
    }

    /* Wait for ALL children before returning to the prompt */
    for (int i = 0; i < spawned; i++) {
        waitpid(pids[i], NULL, 0);
    }

    if (prev_fd != -1) close(prev_fd);
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
