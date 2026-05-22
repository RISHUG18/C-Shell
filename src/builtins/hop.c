/*
 * hop.c — Implementation of the hop builtin (shell cd equivalent).
 *
 * Supports: ~ .. . - <path>
 * Maintains shell_prev_cwd so that 'hop -' works correctly.
 */

#include "../../include/builtins/hop.h"

/* ── Module-level state ─────────────────────────────────────────────────── */
char shell_prev_cwd[MAX_PATH] = "";
int  shell_prev_set           = 0;

/* ── Internal helper: chdir to `path`, update shell_cwd & prev ─────────── */
static void do_hop(const char *path)
{
    /* Save current directory as "previous" before attempting the change */
    char current[MAX_PATH];
    if (getcwd(current, sizeof(current)) == NULL) {
        perror("getcwd");
        return;
    }

    if (chdir(path) != 0) {
        fprintf(stderr, "No such directory!\n");
        return;
    }

    /* Commit: update prev and current cwd globals */
    strncpy(shell_prev_cwd, current, MAX_PATH - 1);
    shell_prev_cwd[MAX_PATH - 1] = '\0';
    shell_prev_set = 1;

    if (getcwd(shell_cwd, sizeof(shell_cwd)) == NULL) {
        perror("getcwd");
    }
}

/* ── builtin_hop ────────────────────────────────────────────────────────── */
void builtin_hop(Command *cmd)
{
    /* No arguments: go home */
    if (cmd->argc < 2) {
        do_hop(shell_home);
        return;
    }

    /* Process each argument left-to-right */
    for (int i = 1; i < cmd->argc; i++) {
        const char *arg = cmd->argv[i];

        if (strcmp(arg, "~") == 0) {
            do_hop(shell_home);

        } else if (strcmp(arg, ".") == 0) {
            /* Stay put — still counts as a successful hop for prev tracking */
            char current[MAX_PATH];
            if (getcwd(current, sizeof(current)) == NULL) { perror("getcwd"); continue; }
            strncpy(shell_prev_cwd, current, MAX_PATH - 1);
            shell_prev_cwd[MAX_PATH - 1] = '\0';
            shell_prev_set = 1;

        } else if (strcmp(arg, "..") == 0) {
            do_hop("..");

        } else if (strcmp(arg, "-") == 0) {
            if (!shell_prev_set) {
                /* No previous directory yet — do nothing */
                continue;
            }
            /* Swap current and prev */
            char temp[MAX_PATH];
            if (getcwd(temp, sizeof(temp)) == NULL) { perror("getcwd"); continue; }

            if (chdir(shell_prev_cwd) != 0) {
                fprintf(stderr, "No such directory!\n");
                continue;
            }

            strncpy(shell_prev_cwd, temp, MAX_PATH - 1);
            shell_prev_cwd[MAX_PATH - 1] = '\0';
            shell_prev_set = 1;

            if (getcwd(shell_cwd, sizeof(shell_cwd)) == NULL) perror("getcwd");

        } else {
            /* Absolute or relative path */
            do_hop(arg);
        }
    }
}
