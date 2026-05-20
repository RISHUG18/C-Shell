
#include "../include/parse.h"

/* ── Internal context passed through all parsing helpers ─────────────────── */
typedef struct {
    char **toks;   /* pointer into tl->tokens array  */
    int    count;  /* total number of tokens          */
    int    pos;    /* current read position           */
} ParseCtx;

/* ── Forward declarations ────────────────────────────────────────────────── */
static int           validate_tokens(ParseCtx *ctx);
static CommandGroup *parse_command_group_list(ParseCtx *ctx);
static CommandGroup *parse_one_group(ParseCtx *ctx, int end, int is_bg);
static Pipeline     *parse_pipeline(ParseCtx *ctx, int start, int end);
static Command      *parse_command(ParseCtx *ctx, int start, int end);

/* ─────────────────────────────────────────────────────────────────────────
 * validate_tokens
 *
 * Single-pass syntax check over the entire token array.
 * Returns 1 if valid, 0 if invalid (and prints "Invalid Syntax!").
 *
 * Rules enforced:
 *   - First token must not be |  ;  &
 *   - Last token must not be |  &  <  >  >>
 *   - A | must be followed by a non-operator token (no empty pipe)
 *   - A | must be preceded by a non-operator token
 *   - Consecutive & or ; are rejected
 *   - < > >> must be immediately followed by a plain name token
 *   - & must appear only at the end of a command group (not mid-pipe)
 * ───────────────────────────────────────────────────────────────────────── */
static int is_redirect(const char *t)
{
    return strcmp(t, "<") == 0 || strcmp(t, ">") == 0 || strcmp(t, ">>") == 0;
}
static int is_operator(const char *t)
{
    return strcmp(t, "|") == 0 || strcmp(t, ";") == 0 ||
           strcmp(t, "&") == 0 || is_redirect(t);
}

static int validate_tokens(ParseCtx *ctx)
{
    int n = ctx->count;
    if (n == 0) return 0;

    /* First token must be a plain word */
    if (is_operator(ctx->toks[0])) {
        fprintf(stderr, "Invalid Syntax!\n");
        return 0;
    }

    /* Last token must not leave something dangling */
    {
        const char *last = ctx->toks[n - 1];
        if (strcmp(last, "|") == 0 || strcmp(last, "&") == 0 ||
            is_redirect(last)) {
            fprintf(stderr, "Invalid Syntax!\n");
            return 0;
        }
    }

    for (int i = 0; i < n; i++) {
        const char *t    = ctx->toks[i];
        const char *prev = (i > 0)     ? ctx->toks[i - 1] : NULL;
        const char *next = (i < n - 1) ? ctx->toks[i + 1] : NULL;

        if (strcmp(t, "|") == 0) {
            /* no empty pipe: token before and after must be plain words */
            if (prev == NULL || is_operator(prev)) {
                fprintf(stderr, "Invalid Syntax!\n"); return 0;
            }
            if (next == NULL || is_operator(next)) {
                fprintf(stderr, "Invalid Syntax!\n"); return 0;
            }
        }

        if (strcmp(t, "&") == 0 || strcmp(t, ";") == 0) {
            /* consecutive separators */
            if (next != NULL &&
                (strcmp(next, "&") == 0 || strcmp(next, ";") == 0)) {
                fprintf(stderr, "Invalid Syntax!\n"); return 0;
            }
            /* & cannot appear inside a pipe segment (prev token is "|") */
            if (strcmp(t, "&") == 0 && prev != NULL &&
                strcmp(prev, "|") == 0) {
                fprintf(stderr, "Invalid Syntax!\n"); return 0;
            }
        }

        if (is_redirect(t)) {
            /* must be followed by a plain filename, not another operator */
            if (next == NULL || is_operator(next)) {
                fprintf(stderr, "Invalid Syntax!\n"); return 0;
            }
        }
    }

    return 1;   /* all checks passed */
}

/* ─────────────────────────────────────────────────────────────────────────
 * parse_command_group_list
 *
 * Top-level: walk the token array, splitting on ';' and '&'.
 * Builds a linked list of CommandGroup nodes.
 * ───────────────────────────────────────────────────────────────────────── */
static CommandGroup *parse_command_group_list(ParseCtx *ctx)
{
    CommandGroup *head = NULL, *tail = NULL;
    int start = 0;

    for (int i = 0; i <= ctx->count; i++) {
        /* treat end-of-tokens as an implicit ';' terminator */
        int is_sep = (i == ctx->count) ||
                     (strcmp(ctx->toks[i], ";") == 0) ||
                     (strcmp(ctx->toks[i], "&") == 0);

        if (!is_sep) continue;

        int is_bg = (i < ctx->count && strcmp(ctx->toks[i], "&") == 0);
        int end   = i - 1;

        /* skip empty groups caused by leading/trailing/consecutive ';' */
        if (start > end) {
            start = i + 1;
            continue;
        }

        ctx->pos = start;
        CommandGroup *cg = parse_one_group(ctx, end, is_bg);
        if (cg == NULL) {
            free_command_group_list(head);
            return NULL;
        }
        cg->is_background = is_bg;

        if (head == NULL) { head = tail = cg; }
        else              { tail->next = cg; tail = cg; }

        start = i + 1;
        ctx->pos = start;
    }

    return head;
}

static CommandGroup *parse_one_group(ParseCtx *ctx, int end, int is_bg)
{
    int start = ctx->pos;

    Pipeline *pl = parse_pipeline(ctx, start, end);
    if (pl == NULL) return NULL;

    CommandGroup *cg = alloc_command_group(pl);
    if (cg == NULL) { free_pipeline(pl); return NULL; }

    cg->is_background = is_bg;
    return cg;
}

static Pipeline *parse_pipeline(ParseCtx *ctx, int start, int end)
{
    Pipeline *pl = alloc_pipeline();
    if (pl == NULL) return NULL;

    Command *head = NULL, *tail = NULL;
    int seg_start = start;

    for (int i = start; i <= end + 1; i++) {
        int is_pipe = (i <= end && strcmp(ctx->toks[i], "|") == 0);
        int is_end  = (i == end + 1);

        if (!is_pipe && !is_end) continue;

        int seg_end = i - 1;

        Command *cmd = parse_command(ctx, seg_start, seg_end);
        if (cmd == NULL) {
            /* free already-built commands attached to head */
            Command *c = head;
            while (c) { Command *n = c->next; free_command(c); c = n; }
            free_pipeline(pl);
            return NULL;
        }

        if (head == NULL) { head = tail = cmd; }
        else              { tail->next = cmd; tail = cmd; }
        pl->cmd_count++;

        seg_start = i + 1;
    }

    pl->head = head;
    return pl;
}

static Command *parse_command(ParseCtx *ctx, int start, int end)
{
    if (start > end) {
        fprintf(stderr, "Invalid Syntax!\n");
        return NULL;
    }

    Command *cmd = alloc_command();
    if (cmd == NULL) return NULL;

    /* First pass: count plain argv slots */
    int argc = 0;
    for (int i = start; i <= end; i++) {
        char *t = ctx->toks[i];
        if (is_redirect(t)) {
            i++;   /* skip filename */
        } else {
            argc++;
        }
    }

    cmd->argv = malloc((argc + 1) * sizeof(char *));
    if (cmd->argv == NULL) { free_command(cmd); return NULL; }
    cmd->argv[argc] = NULL;

    /* Second pass: populate argv + redirections */
    int ai = 0;
    for (int i = start; i <= end; i++) {
        char *t = ctx->toks[i];

        if (strcmp(t, "<") == 0) {
            if (i + 1 > end) { free_command(cmd); return NULL; }
            free(cmd->input_file);
            cmd->input_file = strdup(ctx->toks[++i]);
        } else if (strcmp(t, ">>") == 0) {
            if (i + 1 > end) { free_command(cmd); return NULL; }
            free(cmd->output_file);
            cmd->output_file = strdup(ctx->toks[++i]);
            cmd->append = 1;
        } else if (strcmp(t, ">") == 0) {
            if (i + 1 > end) { free_command(cmd); return NULL; }
            free(cmd->output_file);
            cmd->output_file = strdup(ctx->toks[++i]);
            cmd->append = 0;
        } else {
            cmd->argv[ai++] = strdup(t);
        }
    }
    cmd->argc = ai;
    return cmd;
}

/* ─────────────────────────────────────────────────────────────────────────
 * parse  (public entry point)
 * ───────────────────────────────────────────────────────────────────────── */
CommandGroup *parse(const TokenList *tl)
{
    if (tl == NULL || tl->count == 0) return NULL;

    ParseCtx ctx;
    ctx.toks  = tl->tokens;
    ctx.count = tl->count;
    ctx.pos   = 0;

    /* Validate before building the AST */
    if (!validate_tokens(&ctx)) return NULL;

    return parse_command_group_list(&ctx);
}
