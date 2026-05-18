/*
 * parse.h — Interface for the C-Shell parser.
 *
 * The parser consumes a TokenList produced by the lexer and constructs
 * a linked list of CommandGroup AST nodes that represent the full input.
 *
 * Returns NULL on syntax error (after printing "Invalid Syntax!").
 * The caller must free the returned list with free_command_group_list().
 */

#ifndef PARSE_H
#define PARSE_H

#include "globals.h"
#include "lexer.h"
#include "ast.h"

/*
 * parse - Parse a TokenList into a CommandGroup linked list.
 *
 * Returns:
 *   Pointer to the head CommandGroup on success.
 *   NULL on syntax error or empty input.
 */
CommandGroup *parse(const TokenList *tl);

#endif /* PARSE_H */
