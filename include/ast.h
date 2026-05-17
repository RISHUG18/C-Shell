/*
 * ast.h — Abstract Syntax Tree type definitions for C-Shell.
 *
 * Grammar modelled:
 *
 *   shell_input  ::= command_group ( ( ';' | '&' ) command_group )* [ ';' | '&' ]
 *   command_group::= pipeline
 *   pipeline     ::= command ( '|' command )*
 *   command      ::= NAME arg* redirect*
 *   redirect     ::= ( '<' | '>' | '>>' ) NAME
 *
 * Struct hierarchy (bottom-up):
 *
 *   Command       — one executable with its argv, stdin/stdout redirects
 *   Pipeline      — ordered list of Commands joined by '|'
 *   CommandGroup  — one Pipeline plus whether it runs in background ('&')
 *
 * A full input line is represented as a NULL-terminated linked list of
 * CommandGroup nodes (terminated by ';' or implicit end-of-line).
 */

#ifndef AST_H
#define AST_H

#include "globals.h"

/* ── Command ───────────────────────────────────────────────────────────────
 *
 * Represents a single program invocation, e.g.  grep -i "foo" <in.txt >out.txt
 */
typedef struct Command {
    int    argc;          /* number of arguments (argv[0] = program name)  */
    char **argv;          /* NULL-terminated argument vector                */

    char  *input_file;    /* filename for stdin  redirect (<),  or NULL     */
    char  *output_file;   /* filename for stdout redirect (> / >>), or NULL */
    int    append;        /* 1 = append (>>),  0 = truncate (>)            */

    struct Command *next; /* next command in the pipeline (NULL if last)   */
} Command;

/* ── Pipeline ──────────────────────────────────────────────────────────────
 *
 * An ordered sequence of Commands connected by '|'.
 * head->next->...->NULL
 */
typedef struct Pipeline {
    Command *head;        /* first command in the pipe chain               */
    int      cmd_count;   /* number of commands in the pipeline            */
} Pipeline;

/* ── CommandGroup ──────────────────────────────────────────────────────────
 *
 * One complete pipeline and its execution mode.
 * A shell input line is a linked list of CommandGroups separated by ';'/'&'.
 */
typedef struct CommandGroup {
    Pipeline *pipeline;        /* the pipeline to execute                  */
    int       is_background;   /* 1 = run in background ('&'), 0 = foreground */

    struct CommandGroup *next; /* next group in the input line (NULL if last) */
} CommandGroup;

/* ── Allocation helpers ────────────────────────────────────────────────────
 *
 * All returned pointers must be freed with the matching free_* function.
 * Returns NULL on allocation failure.
 */

/* Allocate a zeroed Command (argc=0, argv=NULL, no redirects) */
Command      *alloc_command(void);

/* Allocate an empty Pipeline (head=NULL, cmd_count=0) */
Pipeline     *alloc_pipeline(void);

/* Allocate a CommandGroup wrapping `pl`, with is_background=0 */
CommandGroup *alloc_command_group(Pipeline *pl);

/* ── Free helpers ──────────────────────────────────────────────────────────
 *
 * Each function recursively frees all owned memory.
 * Safe to call with NULL.
 */
void free_command(Command *cmd);          /* frees one Command (not its ->next) */
void free_pipeline(Pipeline *pl);         /* frees pl and all its commands      */
void free_command_group(CommandGroup *cg);/* frees cg and its pipeline          */
void free_command_group_list(CommandGroup *head); /* frees entire linked list   */

#endif /* AST_H */
