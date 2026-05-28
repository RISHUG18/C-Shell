#include "../include/jobs.h"
#include <string.h>

Job  jobs[MAX_JOBS];
int  job_count   = 0;
int  next_job_num = 1;

int add_job(pid_t pid, const char *name)
{
    if (job_count >= MAX_JOBS) return -1;
    jobs[job_count].job_num = next_job_num++;
    jobs[job_count].pid     = pid;
    jobs[job_count].state   = JOB_RUNNING;
    jobs[job_count].active  = 1;
    strncpy(jobs[job_count].name, name ? name : "?", 255);
    jobs[job_count].name[255] = '\0';
    return job_count++;
}

void remove_job(int idx)
{
    if (idx < 0 || idx >= job_count) return;
    jobs[idx].active = 0;
    for (int i = idx; i < job_count - 1; i++) jobs[i] = jobs[i + 1];
    job_count--;
}

int find_job_by_num(int num)
{
    for (int i = 0; i < job_count; i++)
        if (jobs[i].active && jobs[i].job_num == num) return i;
    return -1;
}

int find_job_by_pid(pid_t pid)
{
    for (int i = 0; i < job_count; i++)
        if (jobs[i].active && jobs[i].pid == pid) return i;
    return -1;
}
