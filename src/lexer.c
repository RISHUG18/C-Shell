#include "../include/lexer.h"

static int append_token(char **arr, int *count, const char *word)
{
    char *copy = strdup(word);
    if (!copy) return -1;
    arr[*count] = copy;
    (*count)++;
    arr[*count] = NULL;
    return 0;
}

static int flush_buf(char *buf, int *len, char **arr, int *count)
{
    if (*len == 0) return 0;
    buf[*len] = '\0';
    int rc = append_token(arr, count, buf);
    *len = 0;
    return rc;
}

TokenList *lexer_tokenise(const char *input)
{
    if (!input) return NULL;

    TokenList *tl = malloc(sizeof(TokenList));
    if (!tl) return NULL;

    tl->tokens = calloc(MAX_TOKENS + 1, sizeof(char *));
    if (!tl->tokens) { free(tl); return NULL; }
    tl->count = 0;

    char buf[MAX_INPUT];
    int  blen = 0;
    const char *p = input;

    while (*p) {
        char c = *p;

        if (c == ' ' || c == '\t' || c == '\r') {
            if (flush_buf(buf, &blen, tl->tokens, &tl->count) < 0) goto oom;
            p++;
            continue;
        }

        if (c == ';' || c == '&' || c == '|' || c == '<' || c == '>') {
            if (flush_buf(buf, &blen, tl->tokens, &tl->count) < 0) goto oom;
            if (c == '>' && *(p + 1) == '>') {
                if (append_token(tl->tokens, &tl->count, ">>") < 0) goto oom;
                p += 2;
            } else {
                char op[2] = { c, '\0' };
                if (append_token(tl->tokens, &tl->count, op) < 0) goto oom;
                p++;
            }
            continue;
        }

        if (blen < (int)(MAX_INPUT - 1)) buf[blen++] = c;
        p++;
    }

    if (flush_buf(buf, &blen, tl->tokens, &tl->count) < 0) goto oom;
    return tl;

oom:
    free_token_list(tl);
    return NULL;
}

void free_token_list(TokenList *tl)
{
    if (!tl) return;
    if (tl->tokens) {
        for (int i = 0; tl->tokens[i]; i++) free(tl->tokens[i]);
        free(tl->tokens);
    }
    free(tl);
}
