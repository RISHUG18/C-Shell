
#include "../include/parse.h"

/* ── Internal context passed through all parsing helpers ─────────────────── */
typedef struct {
    char **toks;   /* pointer into tl->tokens array  */
    int    count;  /* total number of tokens          */
    int    pos;    /* current read position           */
} ParseCtx;

/* ── Forward declarations ────────────────────────────────────────────────── */
static CommandGroup *parse_command_group_list(ParseCtx *ctx);
static CommandGroup *parse_one_group(ParseCtx *ctx, int end, int is_bg);
static Pipeline     *parse_pipeline(ParseCtx *ctx, int start, int end);
static Command      *parse_command(ParseCtx *ctx, int start, int end);

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
        int end   = i - 1;   /* last token index of this group (inclusive) */

        /* skip empty groups caused by leading/trailing/consecutive ';' */
        if (start > end) {
            start = i + 1;
            continue;
        }

        CommandGroup *cg = parse_one_group(ctx, end, is_bg);
        if (cg == NULL) {
            /* parse error — clean up and propagate NULL */
            free_command_group_list(head);
            return NULL;
        }
        cg->is_background = is_bg;

        if (head == NULL) { head = tail = cg; }
        else              { tail->next = cg; tail = cg; }

        start = i + 1;
        /* update ctx->pos so helpers know where we are */
        ctx->pos = start;
    }

    return head;
}

static CommandGroup *parse_one_group(ParseCtx *ctx, int end, int is_bg)
{
    /* ctx->pos holds the start of this group's token range */
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
        /* treat end+1 as an implicit end-of-pipe boundary */
        int is_pipe = (i <= end && strcmp(ctx->toks[i], "|") == 0);
        int is_end  = (i == end + 1);

        if (!is_pipe && !is_end) continue;

        int seg_end = i - 1;

        Command *cmd = parse_command(ctx, seg_start, seg_end);
        if (cmd == NULL) {
            free_pipeline(pl);
            /* free already-built commands */
            Command *c = head;
            while (c) { Command *n = c->next; free_command(c); c = n; }
            return NULL;
        }

        if (head == NULL) { head = tail = cmd; }
        else              { tail->next = cmd; tail = cmd; }
        pl->cmd_count++;

        seg_start = i + 1;   /* skip past the '|' */
    }

    pl->head = head;
    return pl;
}

static Command *parse_command(ParseCtx *ctx, int start, int end)
{
    if (start > end) return NULL;   /* empty segment */

    Command *cmd = alloc_command();
    if (cmd == NULL) return NULL;

    /* First pass: count argv slots (non-redirect tokens) */
    int argc = 0;
    for (int i = start; i <= end; i++) {
        char *t = ctx->toks[i];
        if (strcmp(t, "<") == 0 || strcmp(t, ">") == 0 || strcmp(t, ">>") == 0) {
            i++;   /* skip the filename too */
        } else {
            argc++;
        }
    }

    cmd->argv = malloc((argc + 1) * sizeof(char *));
    if (cmd->argv == NULL) { free_command(cmd); return NULL; }
    cmd->argv[argc] = NULL;

    /* Second pass: fill argv and extract redirections */
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

    return parse_command_group_list(&ctx);
}
