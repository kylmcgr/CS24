/*! \file
 * This file contains the important global definitions for the CS24 Python
 * interpreter.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

/*! History file for Sub-Python */
#define SUBPYTHON_HISTORY ".subpython"

/*! Max depth to print in expressions (in case of reference cycles). */
#define MAX_DEPTH 4

/*! The default size of the memory pool. (Used when -m is not specified.) */
#define DEFAULT_MEMORY_SIZE 1024

/*! Default initial size for realloc-growing arrays. */
#define INITIAL_SIZE 8

/*! A handy macro to delineate an unreachable branch in switches. */
#define UNREACHABLE() \
    do { \
        fprintf(stderr, __FILE__ ":%d - unreachable!\n", __LINE__); \
        abort(); \
    } while (0)

/*! Whether subpython is running in the REPL (versus running a file.) */
extern bool interactive;

#endif /* CONFIG_H */
