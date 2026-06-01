#ifndef BUILTINS_HOP_H
#define BUILTINS_HOP_H

#include "../globals.h"
#include "../ast.h"

extern char shell_prev_cwd[MAX_PATH];
extern int  shell_prev_set;

void builtin_hop(Command *cmd);

#endif
