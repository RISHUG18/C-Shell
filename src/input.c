/*
 * input.c — Safe, POSIX-compliant shell input reader.
 *
 * Reads one line of user input per call:
 *   • fgets-based: bounded, safe against overflow.
 *   • Strips '\n' (and '\r\n' on edge cases).
 *   • Distinguishes EOF, signal interrupts, blank lines, and good input.
 */

#include "../include/input.h"

/* ── read_input ───────────────────────────────────────────────────────────── */
InputStatus read_input(char *buf, size_t bufsz)
{
    if (buf == NULL || bufsz == 0) return INPUT_RETRY;

    errno = 0;
    if (fgets(buf, (int)bufsz, stdin) == NULL) {
        /*
         * fgets returns NULL on two conditions:
         *   1. EOF   — feof(stdin) is true.
         *   2. Error — ferror(stdin) is true; errno may be EINTR if a
         *              signal arrived while we were blocked in the read.
         */
        if (feof(stdin)) {
            /* Ctrl-D: caller should print newline then exit gracefully */
            return INPUT_EOF;
        }

        /* Clear the error state so subsequent reads work after a signal */
        clearerr(stdin);

        if (errno == EINTR) {
            /*
             * A signal (e.g. SIGINT / Ctrl-C) interrupted fgets.
             * We do NOT crash — just tell the caller to re-prompt.
             */
            return INPUT_RETRY;
        }

        /* Other I/O error — treat like a retry to keep the shell alive */
        return INPUT_RETRY;
    }

    /* ── Strip trailing newline / carriage-return ── */
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[--len] = '\0';
    }
    if (len > 0 && buf[len - 1] == '\r') {
        buf[--len] = '\0';
    }

    /* ── Check for blank input (empty or all whitespace) ── */
    for (size_t i = 0; i < len; i++) {
        if (buf[i] != ' ' && buf[i] != '\t') {
            return INPUT_OK;   /* found a non-whitespace character */
        }
    }

    /* Entire line is blank — caller should re-prompt immediately */
    return INPUT_BLANK;
}
