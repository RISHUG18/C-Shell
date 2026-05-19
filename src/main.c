/*
 * main.c — Entry point for C-Shell.
 *
 * REPL pipeline:
 *   1. print_prompt()   — display <user@host:cwd>
 *   2. read_input()     — safe line reader
 *   3. lexer_tokenise() — split into tokens
 *   4. parse()          — build CommandGroup AST
 *   5. execute()        — run the AST (wired in later)
 */

#include "../include/globals.h"
#include "../include/prompt.h"
#include "../include/input.h"
#include "../include/lexer.h"
#include "../include/parse.h"

/* ── Shell-wide state definitions ─────────────────────────────────────────── */
char shell_home[MAX_PATH];
char shell_cwd[MAX_PATH];

/* ── main ─────────────────────────────────────────────────────────────────── */
int main(void)
{
    if (getcwd(shell_home, sizeof(shell_home)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
    strncpy(shell_cwd, shell_home, MAX_PATH - 1);
    shell_cwd[MAX_PATH - 1] = '\0';

    char input[MAX_INPUT];

    while (1) {
        print_prompt();

        InputStatus status = read_input(input, sizeof(input));
        if (status == INPUT_EOF)  { printf("\n"); break; }
        if (status != INPUT_OK)   { continue; }

        TokenList *tl = lexer_tokenise(input);
        if (tl == NULL) { fprintf(stderr, "lexer: allocation failure\n"); continue; }

        CommandGroup *cg_list = parse(tl);
        free_token_list(tl);

        if (cg_list == NULL) {
            /* parse() prints "Invalid Syntax!" when appropriate */
            continue;
        }

        /* TODO: pass cg_list to execute() — wired in next */
        free_command_group_list(cg_list);
    }

    return 0;
}
