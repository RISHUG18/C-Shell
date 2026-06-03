#include "../include/signals.h"
#include "../include/jobs.h"
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>

static void sigint_handler(int sig)  { (void)sig; }
static void sigtstp_handler(int sig) { (void)sig; }

void setup_signals(void)
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);

    /* SIGINT: restart syscalls (fgets retries after Ctrl-C) */
    sa.sa_flags   = SA_RESTART;
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    /* SIGTSTP: do NOT restart — lets fgets return EINTR so the shell
       can re-display the prompt immediately after Ctrl-Z stops a job */
    sa.sa_flags   = 0;
    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, NULL);

    signal(SIGTTOU, SIG_IGN);
    signal(SIGCHLD, SIG_DFL);
}

void check_bg_jobs(void)
{
    int i = 0;
    while (i < job_count) {
        int status;
        pid_t res = waitpid(jobs[i].pid, &status, WNOHANG | WUNTRACED);

        if (res == jobs[i].pid) {
            if (WIFSTOPPED(status)) {
                jobs[i].state = JOB_STOPPED;
                printf("\n[%d] Stopped   %s\n", jobs[i].job_num, jobs[i].name);
                i++;
            } else {
                printf("\n[%d] Done   %s\n", jobs[i].job_num, jobs[i].name);
                remove_job(i);
            }
        } else {
            i++;
        }
    }
}
