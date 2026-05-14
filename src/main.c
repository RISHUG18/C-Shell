/*
 * main.c — Entry point for C-Shell.
 *
 * Responsibilities:
 *   • Initialise shell-wide globals (shell_home, shell_cwd)
 *   • Run the main REPL loop:
 *       1. Print prompt  (prompt.h)
 *       2. Read input
 *       3. (Future days) Parse & execute
 */

#include "../include/globals.h"
#include "../include/prompt.h"

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

        /* A2 — Read input (placeholder: future days will add parsing) */
        if (fgets(input, sizeof(input), stdin) == NULL) {
            /* EOF (Ctrl-D): clean exit */
            if (feof(stdin)) {
                printf("\n");
                break;
            }
            clearerr(stdin);
            continue;
        }

        /* Strip trailing newline */
        input[strcspn(input, "\n")] = '\0';

        /* Skip blank lines */
        int non_blank = 0;
        for (int i = 0; input[i] != '\0'; i++) {
            if (input[i] != ' ' && input[i] != '\t') {
                non_blank = 1;
                break;
            }
        }
        if (!non_blank) continue;

        /*
         * Placeholder: echo the input back.
         * Future days will replace this with tokenisation, parsing,
         * and execution.
         */
        printf("[debug] got: %s\n", input);
    }

    return 0;
}
