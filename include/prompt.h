/*
 * prompt.h — Interface for the shell prompt module.
 *
 * Provides:
 *   print_prompt()   — formats and prints <user@host:cwd>
 *   get_display_cwd()— returns a ~-substituted cwd string
 */

#ifndef PROMPT_H
#define PROMPT_H

#include "globals.h"

/*
 * get_display_cwd - Writes the display form of the current working directory
 *                   into `buf` (size `bufsz`).
 *
 * If cwd is an ancestor of (or equal to) shell_home, the home prefix is
 * replaced by '~'.  Otherwise the absolute path is used.
 *
 * Parameters:
 *   buf    - destination buffer
 *   bufsz  - size of destination buffer
 */
void get_display_cwd(char *buf, size_t bufsz);

/*
 * print_prompt - Prints the shell prompt to stdout:
 *                  <username@hostname:cwd>
 * and flushes stdout so the cursor appears immediately.
 */
void print_prompt(void);

#endif /* PROMPT_H */
