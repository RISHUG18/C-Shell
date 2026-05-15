/*
 * main.c — Entry point for C-Shell.
 *
 * Responsibilities:
 *   • Initialise shell-wide globals (shell_home, shell_cwd)
 *   • Run the main REPL loop:
 *       1. Print prompt           (prompt.h)
 *       2. Read one line of input (input.h)
 *       3. (Future days) Parse & execute tokens
 */

#include "../include/globals.h"
#include "../include/prompt.h"
#include "../include/input.h"

/* ── Shell-wide state definitions ─────────────────────────────────────────── */
char shell_home[MAX_PATH];   /* directory where shell was launched */
char shell_cwd[MAX_PATH];    /* current working directory          */

/* ── main ─────────────────────────────────────────────────────────────────── */
int main(void)
{
    /* Record the startup directory as the shell's "home" for ~ substitution */
    if (getcwd(shell_home, sizeof(shell_home)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    /* Initialise shell_cwd to the same as home at startup */
    strncpy(shell_cwd, shell_home, MAX_PATH - 1);
    shell_cwd[MAX_PATH - 1] = '\0';

    /* ── REPL loop ── */
    char input[MAX_INPUT];

    while (1) {
        /* A1 — Print prompt */
        print_prompt();

        /* A2 — Read one line of input safely */
        InputStatus status = read_input(input, sizeof(input));

        if (status == INPUT_EOF) {
            /* Ctrl-D: graceful exit */
            printf("\n");
            break;
        }
        if (status == INPUT_RETRY || status == INPUT_BLANK) {
            /* Signal interrupt or blank line — re-prompt silently */
            continue;
        }

        /* status == INPUT_OK: we have a non-blank line in `input` */

        /*
         * Placeholder: echo the input back.
         * Future days will replace this with tokenisation, parsing,
         * and execution.
         */
        printf("[debug] got: %s\n", input);
    }

    return 0;
}
