#include "../include/globals.h"
#include "../include/prompt.h"
#include "../include/input.h"
#include "../include/lexer.h"
#include "../include/parse.h"
#include "../include/execute.h"
#include "../include/signals.h"
#include "../include/builtins/log.h"

char shell_home[MAX_PATH];
char shell_cwd[MAX_PATH];

int main(void)
{
    if (getcwd(shell_home, sizeof(shell_home)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }
    strncpy(shell_cwd, shell_home, MAX_PATH - 1);
    shell_cwd[MAX_PATH - 1] = '\0';

    setup_signals();

    char input[MAX_INPUT];

    while (1) {
        check_bg_jobs();
        print_prompt();

        InputStatus status = read_input(input, sizeof(input));
        if (status == INPUT_EOF) { printf("\n"); break; }
        if (status != INPUT_OK)  continue;

        log_record(input);

        TokenList *tl = lexer_tokenise(input);
        if (!tl) { fprintf(stderr, "lexer: allocation failure\n"); continue; }

        CommandGroup *cg = parse(tl);
        free_token_list(tl);
        if (!cg) continue;

        execute_command_group_list(cg);
        free_command_group_list(cg);
    }

    return 0;
}
