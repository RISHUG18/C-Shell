#include "../../include/builtins/hop.h"

char shell_prev_cwd[MAX_PATH] = "";
int  shell_prev_set           = 0;

static void do_hop(const char *path)
{
    char current[MAX_PATH];
    if (getcwd(current, sizeof(current)) == NULL) { perror("getcwd"); return; }

    if (chdir(path) != 0) {
        fprintf(stderr, "No such directory!\n");
        return;
    }

    strncpy(shell_prev_cwd, current, MAX_PATH - 1);
    shell_prev_cwd[MAX_PATH - 1] = '\0';
    shell_prev_set = 1;

    if (getcwd(shell_cwd, sizeof(shell_cwd)) == NULL) perror("getcwd");
}

void builtin_hop(Command *cmd)
{
    if (cmd->argc < 2) { do_hop(shell_home); return; }

    for (int i = 1; i < cmd->argc; i++) {
        const char *arg = cmd->argv[i];

        if (strcmp(arg, "~") == 0) {
            do_hop(shell_home);
        } else if (strcmp(arg, ".") == 0) {
            char cur[MAX_PATH];
            if (getcwd(cur, sizeof(cur)) == NULL) { perror("getcwd"); continue; }
            strncpy(shell_prev_cwd, cur, MAX_PATH - 1);
            shell_prev_cwd[MAX_PATH - 1] = '\0';
            shell_prev_set = 1;
        } else if (strcmp(arg, "..") == 0) {
            do_hop("..");
        } else if (strcmp(arg, "-") == 0) {
            if (!shell_prev_set) continue;
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
            do_hop(arg);
        }
    }
}
