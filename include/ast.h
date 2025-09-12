#include "shell.h"
typedef struct Command{
    char *name;
    char** args;
    struct Command* next;
    int is_background;  // 1 if command should run in background, 0 otherwise
} Command;

Command* create_command(int start,int end);
Command* parse_tokens();