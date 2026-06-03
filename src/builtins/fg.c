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

    /* Copy what we need before the array may shift under us */
    pid_t  jpid    = jobs[idx].pid;
    int    jnum    = jobs[idx].job_num;
    char   jname[256];
    strncpy(jname, jobs[idx].name, 255);
    jname[255] = '\0';

    int tty = open("/dev/tty", O_RDWR);
    if (tty < 0) tty = STDIN_FILENO;

    printf("%s\n", jname);

    if (tcsetpgrp(tty, jpid) < 0) perror("tcsetpgrp");

    kill(-jpid, SIGCONT);
    jobs[idx].state = JOB_RUNNING;
    fg_pgid = jpid;

    int status;
    waitpid(-jpid, &status, WUNTRACED);

    tcsetpgrp(tty, getpgid(0));
    fg_pgid = 0;

    /* Re-find by pid: the index may have shifted if check_bg_jobs ran */
    idx = find_job_by_pid(jpid);

    if (WIFSTOPPED(status)) {
        if (idx >= 0) {
            jobs[idx].state = JOB_STOPPED;
            printf("\n[%d] Stopped   %s\n", jnum, jname);
        }
    } else {
        if (idx >= 0) remove_job(idx);
    }

    if (tty != STDIN_FILENO) close(tty);
}
