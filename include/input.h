/*
 * input.h — Interface for the shell input-reading module.
 *
 * Provides a single safe function to read one line of user input,
 * stripping the newline and handling EOF / signal interrupts cleanly.
 */

#ifndef INPUT_H
#define INPUT_H

#include "globals.h"

/*
 * read_input - Reads one line of input from stdin into `buf`.
 *
 * Behaviour:
 *   • Uses fgets internally (POSIX, safe against buffer overflow).
 *   • Strips the trailing '\n' (and '\r' if present).
 *   • Skips lines that are empty or contain only whitespace — in that
 *     case the function returns INPUT_BLANK so the caller re-prompts.
 *   • On EOF (Ctrl-D) returns INPUT_EOF.
 *   • On a signal-interrupted read (EINTR) returns INPUT_RETRY so the
 *     caller can immediately re-prompt without printing an error.
 *
 * Parameters:
 *   buf    - destination buffer
 *   bufsz  - size of buf (at most bufsz-1 chars are stored)
 *
 * Return values (see enum below):
 */
typedef enum {
    INPUT_OK    =  0,   /* line read successfully, non-blank         */
    INPUT_BLANK =  1,   /* line was empty / all whitespace           */
    INPUT_EOF   = -1,   /* EOF (Ctrl-D)                              */
    INPUT_RETRY = -2    /* interrupted by signal – caller should retry*/
} InputStatus;

InputStatus read_input(char *buf, size_t bufsz);

#endif /* INPUT_H */
