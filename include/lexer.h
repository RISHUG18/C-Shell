#ifndef LEXER_H
#define LEXER_H

#include "globals.h"

#define MAX_TOKENS 1024

typedef struct {
    char **tokens;
    int    count;
} TokenList;

TokenList *lexer_tokenise(const char *input);
void       free_token_list(TokenList *tl);

#endif
