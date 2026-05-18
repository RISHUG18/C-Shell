/*
 * main.c — Entry point for C-Shell.
 *
 * REPL pipeline:
 *   1. print_prompt()        — A1: display <user@host:cwd>
 *   2. read_input()          — A2: safe line reader
 *   3. lexer_tokenise()      — A3: split into tokens
 *   4. parse()               — A3: build CommandGroup AST
 *   5. (future) execute()    — run the AST
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
        if (status == INPUT_EOF)   { printf("\n"); break; }
        if (status != INPUT_OK)    { continue; }

        TokenList *tl = lexer_tokenise(input);
        if (tl == NULL) { fprintf(stderr, "lexer: allocation failure\n"); continue; }

        CommandGroup *cg_list = parse(tl);
        free_token_list(tl);

        if (cg_list == NULL) {
            /* parse() already printed "Invalid Syntax!" if needed */
            continue;
        }

        int g = 0;
        for (CommandGroup *cg = cg_list; cg != NULL; cg = cg->next, g++) {
            printf("[group %d] bg=%d\n", g, cg->is_background);
            int p = 0;
            for (Command *cmd = cg->pipeline->head; cmd != NULL; cmd = cmd->next, p++) {
                printf("  [pipe %d] argc=%d argv[0]=%s\n", p, cmd->argc,
                       cmd->argv && cmd->argv[0] ? cmd->argv[0] : "(null)");
                if (cmd->input_file)  printf("    < %s\n", cmd->input_file);
                if (cmd->output_file) printf("    %s %s\n",
                                             cmd->append ? ">>" : ">",
                                             cmd->output_file);
            }
        }

        free_command_group_list(cg_list);
    }

    return 0;
}
