#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
//llm code starts//
int is_number(const char *s) {
    // Return 0 if string is NULL or empty
    if (!s || !*s) return 0;
    // Iterate through each character in the string
    for (int i = 0; s[i]; ++i) {
        // If any character is not a digit, return 0
        if (!isdigit(s[i])) return 0;
    }
    // All characters are digits, return 1
    return 1;
}

// Sends a signal to a process with the given PID and signal number
void ping_command(const char *pid_str, const char *sig_str) {
    // Validate that both arguments are numbers
    if (!is_number(pid_str) || !is_number(sig_str)) {
        printf("Invalid syntax!\n"); // Print error if not
        return;
    }
    // Convert pid_str to pid_t (process ID)
    pid_t pid = (pid_t)atoi(pid_str);
    // Convert sig_str to integer (signal number)
    int sig = atoi(sig_str);
    // Limit signal number to range 0-31 (standard signals)
    int actual_signal = sig % 32;

    // Check if process with given PID exists (kill with signal 0 does not send a signal)
    if (kill(pid, 0) == -1) {
        // If errno is ESRCH, process does not exist
        if (errno == ESRCH) {
            printf("No such process found\n");
            return;
        }
    }
    // Try to send the actual signal to the process
    if (kill(pid, actual_signal) == 0) {
        // Signal sent successfully
        printf("Sent signal %d to process with pid %d\n", sig, pid);
    } else {
        // If sending signal failed
        // EPERM: No permission to send signal
        // ESRCH: Process does not exist
        if (errno == ESRCH) {
            printf("No such process found\n");
        } else {
            // Print error message for other errors
            perror("kill");
        }
    }
}
//llm code ends//