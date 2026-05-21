/*
 * execute.h — Interface for the C-Shell command executor.
 */

#ifndef EXECUTE_H
#define EXECUTE_H

#include "globals.h"
#include "ast.h"

/*
 * execute_command_group_list - Execute a linked list of CommandGroups.
 *
 * Iterates over each CommandGroup, runs its pipeline either in the
 * foreground (is_background=0) or background (is_background=1).
 */
void execute_command_group_list(CommandGroup *head);

/*
 * execute_pipeline - Fork and execute a single Pipeline.
 * Handles pipes between commands and per-command redirections.
 * Blocks until the pipeline finishes (foreground call).
 */
void execute_pipeline(Pipeline *pl);

/*
 * execute_single - Execute one Command with no pipes.
 * Applies redirections, forks, calls execvp in the child.
 * Returns the child's exit status via *status (if non-NULL).
 */
void execute_single(Command *cmd, int *out_status);

#endif /* EXECUTE_H */
