#ifndef AST_H
#define AST_H

#include "globals.h"

typedef struct Command {
    int    argc;
    char **argv;
    char  *input_file;
    char  *output_file;
    int    append;
    struct Command *next;
} Command;

typedef struct Pipeline {
    Command *head;
    int      cmd_count;
} Pipeline;

typedef struct CommandGroup {
    Pipeline *pipeline;
    int       is_background;
    struct CommandGroup *next;
} CommandGroup;

Command      *alloc_command(void);
Pipeline     *alloc_pipeline(void);
CommandGroup *alloc_command_group(Pipeline *pl);

void free_command(Command *cmd);
void free_pipeline(Pipeline *pl);
void free_command_group(CommandGroup *cg);
void free_command_group_list(CommandGroup *head);

#endif
