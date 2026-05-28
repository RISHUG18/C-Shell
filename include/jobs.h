#ifndef JOBS_H
#define JOBS_H

#include "globals.h"
#include <sys/types.h>

#define MAX_JOBS 128

typedef enum { JOB_RUNNING, JOB_STOPPED, JOB_DONE } JobState;

typedef struct {
    int      job_num;
    pid_t    pid;
    JobState state;
    char     name[256];
    int      active;
} Job;

extern Job    jobs[MAX_JOBS];
extern int    job_count;
extern int    next_job_num;

int  add_job(pid_t pid, const char *name);
void remove_job(int idx);
int  find_job_by_num(int num);
int  find_job_by_pid(pid_t pid);

#endif
