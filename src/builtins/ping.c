#include "../../include/builtins/ping.h"
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int all_digits(const char *s)
{
    if (!s || !*s) return 0;
    for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

void builtin_ping(Command *cmd)
{
    if (cmd->argc < 3) {
        fprintf(stderr, "ping: usage: ping <pid> <signal>\n");
        return;
    }

    if (!all_digits(cmd->argv[1]) || !all_digits(cmd->argv[2])) {
        fprintf(stderr, "ping: invalid arguments\n");
        return;
    }

    pid_t pid = (pid_t)atoi(cmd->argv[1]);
    int   sig = atoi(cmd->argv[2]) % 32;

    if (kill(pid, 0) < 0 && errno == ESRCH) {
        fprintf(stderr, "ping: no such process\n");
        return;
    }

    if (kill(pid, sig) == 0) {
        printf("Sent signal %d to process with pid %d\n", atoi(cmd->argv[2]), pid);
    } else {
        if (errno == ESRCH) fprintf(stderr, "ping: no such process\n");
        else                perror("kill");
    }
}
