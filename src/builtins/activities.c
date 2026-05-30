#include "../../include/builtins/activities.h"
#include "../../include/jobs.h"
#include <string.h>
#include <stdlib.h>

static int cmp_job_name(const void *a, const void *b)
{
    return strcmp(((Job *)a)->name, ((Job *)b)->name);
}

void builtin_activities(Command *cmd)
{
    (void)cmd;
    if (job_count == 0) return;

    Job sorted[MAX_JOBS];
    int n = 0;
    for (int i = 0; i < job_count; i++)
        if (jobs[i].active) sorted[n++] = jobs[i];

    qsort(sorted, n, sizeof(Job), cmp_job_name);

    for (int i = 0; i < n; i++) {
        const char *state = (sorted[i].state == JOB_STOPPED) ? "Stopped" : "Running";
        printf("[%d] %d - %s - %s\n",
               sorted[i].job_num, sorted[i].pid, state, sorted[i].name);
    }
}
