/*
 * ast.c — Allocation and deallocation for C-Shell AST nodes.
 *
 * Only memory management lives here.  Parsing logic is in parse.c.
 */

#include "../include/ast.h"

/* ── alloc_command ─────────────────────────────────────────────────────── */
Command *alloc_command(void)
{
    Command *cmd = calloc(1, sizeof(Command));
    /* calloc zeroes the struct: argc=0, argv=NULL, files=NULL, append=0 */
    return cmd;   /* NULL on allocation failure */
}

/* ── alloc_pipeline ────────────────────────────────────────────────────── */
Pipeline *alloc_pipeline(void)
{
    Pipeline *pl = calloc(1, sizeof(Pipeline));
    return pl;
}

/* ── alloc_command_group ───────────────────────────────────────────────── */
CommandGroup *alloc_command_group(Pipeline *pl)
{
    CommandGroup *cg = calloc(1, sizeof(CommandGroup));
    if (cg != NULL) {
        cg->pipeline      = pl;
        cg->is_background = 0;
        cg->next          = NULL;
    }
    return cg;
}

/* ── free_command ──────────────────────────────────────────────────────── */
/*
 * Frees all memory owned by `cmd` but does NOT follow cmd->next.
 * Use free_pipeline() to free an entire chain.
 */
void free_command(Command *cmd)
{
    if (cmd == NULL) return;

    if (cmd->argv != NULL) {
        for (int i = 0; i < cmd->argc; i++) {
            free(cmd->argv[i]);
        }
        free(cmd->argv);
    }

    free(cmd->input_file);    /* free(NULL) is safe per POSIX */
    free(cmd->output_file);
    free(cmd);
}

/* ── free_pipeline ─────────────────────────────────────────────────────── */
void free_pipeline(Pipeline *pl)
{
    if (pl == NULL) return;

    Command *cur = pl->head;
    while (cur != NULL) {
        Command *nxt = cur->next;
        free_command(cur);
        cur = nxt;
    }

    free(pl);
}

/* ── free_command_group ────────────────────────────────────────────────── */
/* Frees one CommandGroup and its pipeline, but NOT cg->next. */
void free_command_group(CommandGroup *cg)
{
    if (cg == NULL) return;
    free_pipeline(cg->pipeline);
    free(cg);
}

/* ── free_command_group_list ───────────────────────────────────────────── */
/* Frees an entire linked list of CommandGroups. */
void free_command_group_list(CommandGroup *head)
{
    while (head != NULL) {
        CommandGroup *nxt = head->next;
        free_command_group(head);
        head = nxt;
    }
}
