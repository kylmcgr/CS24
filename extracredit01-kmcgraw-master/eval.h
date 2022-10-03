#ifndef EVAL_H
#define EVAL_H

#include "ast.h"
#include "types.h"

void eval_init(void);
void eval_close(void);
reference_t eval_root(Node *root);

/*!
 * Calls a function for each global variable.
 * The function is passed the name of the global and the reference of its value.
 * Returns the number of globals.
 */
size_t foreach_global(void (*f)(char *name, reference_t ref));
void print_globals(void);

#endif /* EVAL_H */
