/*
 * globals.h — Shared constants, macros, and extern declarations
 *             for the C-Shell project.
 */

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

/* ── Buffer sizes ─────────────────────────────────────────── */
#define MAX_INPUT    4096   /* max raw input line length        */
#define MAX_PATH     4096   /* max path length                  */
#define MAX_HOSTNAME 256    /* max hostname length              */
#define MAX_USERNAME 256    /* max username length              */

/* ── Shell-wide state (defined in main.c) ─────────────────── */
extern char shell_home[MAX_PATH];   /* directory where shell was launched */
extern char shell_cwd[MAX_PATH];    /* current working directory          */

#endif /* GLOBALS_H */
