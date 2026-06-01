#ifndef PROMPT_H
#define PROMPT_H

#include "globals.h"
#include <pwd.h>

void get_display_cwd(char *buf, size_t bufsz);
void print_prompt(void);

#endif
