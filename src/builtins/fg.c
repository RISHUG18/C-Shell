#include "../../include/builtins/fg.h"
#include "../../include/jobs.h"
#include "../../include/execute.h"
#include <sys/wait.h>
#include <termios.h>
#include <fcntl.h>

void builtin_fg(Command *cmd)
{
    int num = 1;
    if (cmd->argc >= 2) num = atoi(cmd->argv[1]);

    int idx = find_job_by_num(num);
    if (idx < 0) {
        fprintf(stderr, "fg: no such job [%d]\n", num);
        return;
    }

    Job *j = &jobs[idx];
    int tty = open("/dev/tty", O_RDWR);
    if (tty < 0) tty = STDIN_FILENO;

    printf("%s\n", j->name);

    if (tcsetpgrp(tty, j->pid) < 0) perror("tcsetpgrp");

    kill(-j->pid, SIGCONT);
    j->state = JOB_RUNNING;
    fg_pgid   = j->pid;

    int status;
    waitpid(-j->pid, &status, WUNTRACED);

    tcsetpgrp(tty, getpgid(0));
    fg_pgid = 0;

    if (WIFSTOPPED(status)) {
        j->state = JOB_STOPPED;
        printf("\n[%d] Stopped   %s\n", j->job_num, j->name);
    } else {
        remove_job(idx);
    }

    if (tty != STDIN_FILENO) close(tty);
}
