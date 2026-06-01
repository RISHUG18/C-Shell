#ifndef GLOBALS_H
#define GLOBALS_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_INPUT    4096
#define MAX_PATH     4096
#define MAX_HOSTNAME 256
#define MAX_USERNAME 256

extern char shell_home[MAX_PATH];
extern char shell_cwd[MAX_PATH];
extern pid_t fg_pgid;

#endif
