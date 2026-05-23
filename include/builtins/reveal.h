/*
 * reveal.h — Interface for the reveal builtin (ls equivalent).
 */

#ifndef BUILTINS_REVEAL_H
#define BUILTINS_REVEAL_H

#include "../globals.h"
#include "../ast.h"

/*
 * builtin_reveal - List directory contents.
 *
 * Flags: -a (show hidden files), -l (long format, one per line).
 * Flags may be combined: -la, -al.
 * Path defaults to cwd. Supports ~, ., .., -, absolute/relative paths.
 * Files are sorted lexicographically (ASCII/strcmp order).
 */
void builtin_reveal(Command *cmd);

#endif /* BUILTINS_REVEAL_H */
