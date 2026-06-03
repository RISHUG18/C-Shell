#include "../../include/builtins/bg.h"
#include "../../include/jobs.h"

void builtin_bg(Command *cmd)
{
    int num = 1;
    if (cmd->argc >= 2) num = atoi(cmd->argv[1]);

    int idx = find_job_by_num(num);
    if (idx < 0) {
        fprintf(stderr, "bg: no such job [%d]\n", num);
        return;
    }

    Job *j = &jobs[idx];
    if (j->state == JOB_RUNNING) {
        fprintf(stderr, "bg: job [%d] is already running\n", num);
        return;
    }

    j->state = JOB_RUNNING;
    printf("[%d] %s\n", j->job_num, j->name);
    kill(-j->pid, SIGCONT);
}
