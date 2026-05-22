/*
 * hop.h — Interface for the hop builtin (cd equivalent).
 */

#ifndef BUILTINS_HOP_H
#define BUILTINS_HOP_H

#include "../globals.h"
#include "../ast.h"

/* Shell-wide previous-directory state (defined in hop.c) */
extern char shell_prev_cwd[MAX_PATH];
extern int  shell_prev_set;   /* 0 until the first successful hop */

/*
 * builtin_hop - Change directory according to hop semantics.
 *
 * With no arguments: hop to shell_home.
 * Processes each argument left-to-right:
 *   ~        → shell_home
 *   .        → stay in current directory (no-op)
 *   ..       → parent directory
 *   -        → previous directory (shell_prev_cwd); error if never set
 *   anything else → chdir() to that path (absolute or relative)
 *
 * On chdir failure: prints "No such directory!" and continues.
 */
void builtin_hop(Command *cmd);

#endif /* BUILTINS_HOP_H */
