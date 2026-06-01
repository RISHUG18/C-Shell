#include "../include/input.h"

InputStatus read_input(char *buf, size_t bufsz)
{
    if (!buf || bufsz == 0) return INPUT_RETRY;

    errno = 0;
    if (fgets(buf, (int)bufsz, stdin) == NULL) {
        if (feof(stdin)) return INPUT_EOF;
        clearerr(stdin);
        return INPUT_RETRY;
    }

    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';
    if (len > 0 && buf[len - 1] == '\r') buf[--len] = '\0';

    for (size_t i = 0; i < len; i++)
        if (buf[i] != ' ' && buf[i] != '\t') return INPUT_OK;

    return INPUT_BLANK;
}
