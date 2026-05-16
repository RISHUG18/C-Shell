/*
 * lexer.c — Lexical tokeniser for C-Shell.
 *
 * Algorithm overview:
 *   1. Walk through the input character-by-character.
 *   2. Accumulate non-operator characters into a growing buffer.
 *   3. When an operator character (;  &  |  <  >) is encountered:
 *        a. Flush any accumulated buffer as a token.
 *        b. Peek ahead: '>>' is a single two-character token.
 *        c. Emit the operator as its own token.
 *   4. Whitespace (space, tab, \r) flushes the buffer but emits nothing.
 *   5. All tokens are heap-allocated (strdup); the caller owns them.
 *
 * This approach correctly handles zero-space operator adjacency:
 *   "cmd<in.txt"    → ["cmd", "<", "in.txt"]
 *   "a>>b"          → ["a", ">>", "b"]
 *   "ls|grep foo"   → ["ls", "|", "grep", "foo"]
 */

#include "../include/lexer.h"

/* ── Internal helpers ─────────────────────────────────────────────────────── */

/* Grow the token array by one slot and store a copy of `word`. */
static int append_token(char **arr, int *count, const char *word)
{
    char *copy = strdup(word);
    if (copy == NULL) return -1;          /* allocation failure */
    arr[*count] = copy;
    (*count)++;
    arr[*count] = NULL;                   /* keep NULL-terminated */
    return 0;
}

/* Flush a char buffer as a token if it contains anything. */
static int flush_buf(char *buf, int *buf_len, char **arr, int *count)
{
    if (*buf_len == 0) return 0;
    buf[*buf_len] = '\0';
    int rc = append_token(arr, count, buf);
    *buf_len = 0;
    return rc;
}

/* ── lexer_tokenise ──────────────────────────────────────────────────────── */
TokenList *lexer_tokenise(const char *input)
{
    if (input == NULL) return NULL;

    /* Allocate the result struct */
    TokenList *tl = malloc(sizeof(TokenList));
    if (tl == NULL) return NULL;

    /* Allocate the token pointer array (+1 for NULL sentinel) */
    tl->tokens = calloc(MAX_TOKENS + 1, sizeof(char *));
    if (tl->tokens == NULL) { free(tl); return NULL; }
    tl->count = 0;

    /* Working buffer for the current word being accumulated */
    char buf[MAX_INPUT];
    int  buf_len = 0;

    const char *p = input;

    while (*p != '\0') {
        char c = *p;

        /* ── Whitespace: flush word, advance ── */
        if (c == ' ' || c == '\t' || c == '\r') {
            if (flush_buf(buf, &buf_len, tl->tokens, &tl->count) < 0) goto oom;
            p++;
            continue;
        }

        /* ── Operators ── */
        if (c == ';' || c == '&' || c == '|' || c == '<' || c == '>') {
            /* First flush any accumulated word */
            if (flush_buf(buf, &buf_len, tl->tokens, &tl->count) < 0) goto oom;

            /* Special case: ">>" is a single two-character operator */
            if (c == '>' && *(p + 1) == '>') {
                if (append_token(tl->tokens, &tl->count, ">>") < 0) goto oom;
                p += 2;
            } else {
                /* Single-character operator */
                char op[2] = { c, '\0' };
                if (append_token(tl->tokens, &tl->count, op) < 0) goto oom;
                p++;
            }
            continue;
        }

        /* ── Regular character: accumulate into buffer ── */
        if (buf_len < (int)(MAX_INPUT - 1)) {
            buf[buf_len++] = c;
        }
        /* If buffer is full we silently truncate (input > MAX_INPUT is
           pathological; no crash is the priority). */
        p++;
    }

    /* Flush any remaining word after the loop ends */
    if (flush_buf(buf, &buf_len, tl->tokens, &tl->count) < 0) goto oom;

    return tl;

oom:
    /* On any allocation failure, clean up everything and return NULL */
    free_token_list(tl);
    return NULL;
}

/* ── free_token_list ─────────────────────────────────────────────────────── */
void free_token_list(TokenList *tl)
{
    if (tl == NULL) return;
    if (tl->tokens != NULL) {
        for (int i = 0; tl->tokens[i] != NULL; i++) {
            free(tl->tokens[i]);
        }
        free(tl->tokens);
    }
    free(tl);
}
