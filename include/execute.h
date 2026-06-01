#ifndef EXECUTE_H
#define EXECUTE_H

#include "globals.h"
#include "ast.h"

void init_execute(void);
void execute_single(Command *cmd, int *out_status);
void execute_pipeline(Pipeline *pl);
void execute_command_group_list(CommandGroup *head);

#endif
