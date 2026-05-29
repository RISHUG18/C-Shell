#include "../include/signals.h"
#include "../include/jobs.h"
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>

/* shell ignores Ctrl-C so only foreground children are affected */
static void sigint_handler(int sig)
{
    (void)sig;
}

static void sigtstp_handler(int sig)
{
    (void)sig;
}

void setup_signals(void)
{
    struct sigaction sa;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, NULL);

    /* ignore SIGTTOU so background jobs don't stall on tty writes */
    signal(SIGTTOU, SIG_IGN);
}

void check_bg_jobs(void)
{
    int i = 0;
    while (i < job_count) {
        Job *j = &jobs[i];
        if (!j->active) { i++; continue; }

        int status;
        pid_t res = waitpid(j->pid, &status, WNOHANG | WUNTRACED);

        if (res == j->pid) {
            if (WIFSTOPPED(status)) {
                j->state = JOB_STOPPED;
                printf("\n[%d] Stopped   %s\n", j->job_num, j->name);
                i++;
            } else {
                printf("\n[%d] Done   %s\n", j->job_num, j->name);
                remove_job(i);
                /* don't increment i — remove_job compacts the array */
            }
        } else {
            i++;
        }
    }
}
