#include "tokenize.h"
#include "shell.h"

//gpt code starts//
int tokenize_input(char *input) {
    int count = 0;
    char *token = strtok(input, " \t\r\n"); // Split by spaces, tabs, and newline
   while (token != NULL && count < MAX_TOKENS) {
    char *p = token;   // pointer to walk through token
    char buffer[1024];
    int buf_i = 0;

    while (*p != '\0') {
        if (*p == ';' || *p=='|' || *p == '&') {
            // flush anything collected before separator
            if (buf_i > 0) {
                buffer[buf_i] = '\0';
                tokens[count++] = strdup(buffer);
                buf_i = 0;
            }

            // add separator as its own token
            if(*p == ';') tokens[count++]=strdup(";");
            if(*p=='|') tokens[count++]=strdup("|");
            if(*p=='&') tokens[count++]=strdup("&");
        } else {
            buffer[buf_i++] = *p;
        }
        p++;
    }

    // flush whatever is left in buffer
    if (buf_i > 0) {
        buffer[buf_i] = '\0';
        tokens[count++] = strdup(buffer);
    }

    token = strtok(NULL, " \t\r\n"); // continue tokenizing
}

    tokens[count] = NULL; // NULL-terminate the token array
    return count;
}
//gpt code ends;