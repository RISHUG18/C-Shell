/*
 * log.h — Interface for the log builtin (command history).
 */

#ifndef BUILTINS_LOG_H
#define BUILTINS_LOG_H

#include "../globals.h"
#include "../ast.h"

#define LOG_MAX_ENTRIES  15
#define LOG_FILE         "log.txt"

/*
 * log_record - Persist `input_line` to LOG_FILE.
 *
 * Rules:
 *   - Commands containing "log" are not recorded.
 *   - Consecutive duplicate commands are not recorded.
 *   - At most LOG_MAX_ENTRIES lines are kept (oldest dropped when full).
 *
 * Call this in main.c after every successful input read, before execute.
 */
void log_record(const char *input_line);

/*
 * builtin_log - The log builtin command.
 *
 *   log           — print history (most recent last)
 *   log purge     — clear the history file
 *   log execute N — re-run the N-th most recent entry (1 = most recent)
 */
void builtin_log(Command *cmd);

#endif /* BUILTINS_LOG_H */
