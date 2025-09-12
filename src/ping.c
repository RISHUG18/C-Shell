#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
//llm code starts//
int is_number(const char *s) {
    if (!s || !*s) return 0;
    for (int i = 0; s[i]; ++i) {
        if (!isdigit(s[i])) return 0;
    }
    return 1;
}

void ping_command(const char *pid_str, const char *sig_str) {
    if (!is_number(pid_str) || !is_number(sig_str)) {
        printf("Invalid syntax!\n");
        return;
    }
    pid_t pid = (pid_t)atoi(pid_str);
    int sig = atoi(sig_str);
    int actual_signal = sig % 32;

    // Check if process exists
    if (kill(pid, 0) == -1) {
        if (errno == ESRCH) {
            printf("No such process found\n");
            return;
        }
    }
    // Try to send the signal
    if (kill(pid, actual_signal) == 0) {
        printf("Sent signal %d to process with pid %d\n", sig, pid);
    } else {
        // EPERM might mean the process exists but we can't signal it.
        // ESRCH means it's gone.
        if (errno == ESRCH) {
            printf("No such process found\n");
        } else {
            perror("kill");
        }
    }
}
//llm code ends//