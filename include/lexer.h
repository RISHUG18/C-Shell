/*
 * lexer.h — Interface for the C-Shell lexical tokeniser.
 *
 * Splits a raw input string into an array of string tokens, correctly
 * isolating shell operators even when they appear with no surrounding
 * whitespace (e.g. "cmd<in.txt" → ["cmd", "<", "in.txt"]).
 *
 * Operators recognised as standalone tokens:
 *   >>   (must be checked before >)
 *   >    <    |    ;    &
 */

#ifndef LEXER_H
#define LEXER_H

#include "globals.h"

/* Maximum number of tokens produced from a single input line */
#define MAX_TOKENS 1024

/*
 * TokenList — heap-allocated result of tokenising one input line.
 *
 * tokens  : NULL-terminated array of NUL-terminated strings.
 *           Each string is independently heap-allocated.
 * count   : number of tokens (excluding the final NULL sentinel).
 *
 * Call free_token_list() when done to avoid leaks.
 */
typedef struct {
    char **tokens;   /* NULL-terminated token array */
    int    count;    /* number of valid tokens       */
} TokenList;

/*
 * lexer_tokenise - Tokenises `input` and returns a heap-allocated TokenList.
 *
 * The caller must free the result with free_token_list().
 * Returns NULL only on allocation failure.
 *
 * `input` is NOT modified (a working copy is made internally).
 */
TokenList *lexer_tokenise(const char *input);

/*
 * free_token_list - Frees every string in tl->tokens, then tl itself.
 * Safe to call with NULL.
 */
void free_token_list(TokenList *tl);

#endif /* LEXER_H */
