#ifndef INPUT_H
#define INPUT_H

#include "globals.h"

typedef enum {
    INPUT_OK    =  0,
    INPUT_BLANK =  1,
    INPUT_EOF   = -1,
    INPUT_RETRY = -2
} InputStatus;

InputStatus read_input(char *buf, size_t bufsz);

#endif
