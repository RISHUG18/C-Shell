#include "../include/parse.h"

typedef struct {
    char **toks;
    int    count;
    int    pos;
} ParseCtx;

static int           validate_tokens(ParseCtx *ctx);
static CommandGroup *parse_command_group_list(ParseCtx *ctx);
static CommandGroup *parse_one_group(ParseCtx *ctx, int end, int is_bg);
static Pipeline     *parse_pipeline(ParseCtx *ctx, int start, int end);
static Command      *parse_command(ParseCtx *ctx, int start, int end);

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

    if (is_operator(ctx->toks[0])) { fprintf(stderr, "Invalid Syntax!\n"); return 0; }

    const char *last = ctx->toks[n - 1];
    if (strcmp(last, "|") == 0 || strcmp(last, "&") == 0 || is_redirect(last)) {
        fprintf(stderr, "Invalid Syntax!\n"); return 0;
    }

    for (int i = 0; i < n; i++) {
        const char *t    = ctx->toks[i];
        const char *prev = (i > 0)     ? ctx->toks[i - 1] : NULL;
        const char *next = (i < n - 1) ? ctx->toks[i + 1] : NULL;

        if (strcmp(t, "|") == 0) {
            if (!prev || is_operator(prev) || !next || is_operator(next)) {
                fprintf(stderr, "Invalid Syntax!\n"); return 0;
            }
        }
        if (strcmp(t, "&") == 0 || strcmp(t, ";") == 0) {
            if (next && (strcmp(next, "&") == 0 || strcmp(next, ";") == 0)) {
                fprintf(stderr, "Invalid Syntax!\n"); return 0;
            }
            if (strcmp(t, "&") == 0 && prev && strcmp(prev, "|") == 0) {
                fprintf(stderr, "Invalid Syntax!\n"); return 0;
            }
        }
        if (is_redirect(t)) {
            if (!next || is_operator(next)) {
                fprintf(stderr, "Invalid Syntax!\n"); return 0;
            }
        }
    }
    return 1;
}

static CommandGroup *parse_command_group_list(ParseCtx *ctx)
{
    CommandGroup *head = NULL, *tail = NULL;
    int start = 0;

    for (int i = 0; i <= ctx->count; i++) {
        int is_sep = (i == ctx->count) ||
                     (strcmp(ctx->toks[i], ";") == 0) ||
                     (strcmp(ctx->toks[i], "&") == 0);

        if (!is_sep) continue;

        int is_bg = (i < ctx->count && strcmp(ctx->toks[i], "&") == 0);
        int end   = i - 1;

        if (start > end) { start = i + 1; continue; }

        ctx->pos = start;
        CommandGroup *cg = parse_one_group(ctx, end, is_bg);
        if (!cg) { free_command_group_list(head); return NULL; }
        cg->is_background = is_bg;

        if (!head) head = tail = cg;
        else { tail->next = cg; tail = cg; }

        start = i + 1;
        ctx->pos = start;
    }
    return head;
}

static CommandGroup *parse_one_group(ParseCtx *ctx, int end, int is_bg)
{
    Pipeline *pl = parse_pipeline(ctx, ctx->pos, end);
    if (!pl) return NULL;

    CommandGroup *cg = alloc_command_group(pl);
    if (!cg) { free_pipeline(pl); return NULL; }
    cg->is_background = is_bg;
    return cg;
}

static Pipeline *parse_pipeline(ParseCtx *ctx, int start, int end)
{
    Pipeline *pl = alloc_pipeline();
    if (!pl) return NULL;

    Command *head = NULL, *tail = NULL;
    int seg_start = start;

    for (int i = start; i <= end + 1; i++) {
        int is_pipe = (i <= end && strcmp(ctx->toks[i], "|") == 0);
        int is_end  = (i == end + 1);
        if (!is_pipe && !is_end) continue;

        Command *cmd = parse_command(ctx, seg_start, i - 1);
        if (!cmd) {
            Command *c = head;
            while (c) { Command *nx = c->next; free_command(c); c = nx; }
            free_pipeline(pl);
            return NULL;
        }
        if (!head) head = tail = cmd;
        else { tail->next = cmd; tail = cmd; }
        pl->cmd_count++;
        seg_start = i + 1;
    }

    pl->head = head;
    return pl;
}

static Command *parse_command(ParseCtx *ctx, int start, int end)
{
    if (start > end) { fprintf(stderr, "Invalid Syntax!\n"); return NULL; }

    Command *cmd = alloc_command();
    if (!cmd) return NULL;

    int argc = 0;
    for (int i = start; i <= end; i++) {
        if (is_redirect(ctx->toks[i])) i++;
        else argc++;
    }

    cmd->argv = malloc((argc + 1) * sizeof(char *));
    if (!cmd->argv) { free_command(cmd); return NULL; }
    cmd->argv[argc] = NULL;

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

CommandGroup *parse(const TokenList *tl)
{
    if (!tl || tl->count == 0) return NULL;

    ParseCtx ctx;
    ctx.toks  = tl->tokens;
    ctx.count = tl->count;
    ctx.pos   = 0;

    if (!validate_tokens(&ctx)) return NULL;
    return parse_command_group_list(&ctx);
}
