#include "../include/ast.h"

Command *alloc_command(void)
{
    return calloc(1, sizeof(Command));
}

Pipeline *alloc_pipeline(void)
{
    return calloc(1, sizeof(Pipeline));
}

CommandGroup *alloc_command_group(Pipeline *pl)
{
    CommandGroup *cg = calloc(1, sizeof(CommandGroup));
    if (cg) {
        cg->pipeline      = pl;
        cg->is_background = 0;
        cg->next          = NULL;
    }
    return cg;
}

void free_command(Command *cmd)
{
    if (!cmd) return;
    if (cmd->argv) {
        for (int i = 0; i < cmd->argc; i++) free(cmd->argv[i]);
        free(cmd->argv);
    }
    free(cmd->input_file);
    free(cmd->output_file);
    free(cmd);
}

void free_pipeline(Pipeline *pl)
{
    if (!pl) return;
    Command *cur = pl->head;
    while (cur) {
        Command *nxt = cur->next;
        free_command(cur);
        cur = nxt;
    }
    free(pl);
}

void free_command_group(CommandGroup *cg)
{
    if (!cg) return;
    free_pipeline(cg->pipeline);
    free(cg);
}

void free_command_group_list(CommandGroup *head)
{
    while (head) {
        CommandGroup *nxt = head->next;
        free_command_group(head);
        head = nxt;
    }
}
