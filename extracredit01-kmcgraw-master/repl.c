/*! \file
 * This file implements the Read-Eval-Print Loop (REPL) for the simple CS24
 * Python interpreter.
 */

#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef NREADLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "config.h"
#include "eval.h"
#include "eval_types.h"
#include "exception.h"
#include "mm.h"
#include "parser.h"
#include "refs.h"

bool interactive;
static bool debug = false;

/*!
 * Helper function that calls into the evaluation system to evaluate the
 * provided AST node. Returns whether or not the AST was executed to
 * completion.
 */
bool eval(Node *node) {
    if (node == NULL) {
        /* Parsed a comment; nothing to do. */
        return true;
    }

    /* Perform the computation. */
    reference_t result = eval_root(node);

    /* Check if there is an exception. */
    bool completed = !exception_occurred();
    if (completed) {
        if (!ref_is_none(result)) {
            ref_println_repr(result, stdout, MAX_DEPTH);
        }

        /* Now, make sure to release the final output! */
        decref(result);
    } else {
        exception_print(stderr);
        exception_clear();
        assert(result == NULL_REF);
    }

    if (debug) {
        printf("\n");

        print_globals();

        printf("\nMemory Contents:\n");
        mem_dump(stdout);

        printf("\n");
    }

    return completed;
}

/*!
 * This enumeration represents the set of actions that the REPL should take
 * after the parsing and execution of a complete user input.
 */
typedef enum {
    REPL_ACTION_CONTINUE,
    REPL_ACTION_ERROR,
    REPL_ACTION_EXIT
} repl_action_t;

/*!
 * This is the function for handling code files or scripts. This is used
 * anytime the input is determined to be non-interactive.
 */
repl_action_t try_parse(FILE *input) {
    /* Attempt to parse the contents of the stream. */
    parse_result_t result = parse(interactive, input);
    parse_result_type_t result_type = result.type;

    /* If the parse was successful, then evaluate the produced AST. */
    bool complete = result.type == RESULT_SUCCESS && eval(result.ast.root);

    /* Cleanup the parse result. */
    parse_result_destroy(&result);

    /* Return action type so that we can check if exit was requested. */
    if (complete) {
        return REPL_ACTION_CONTINUE;
    } else if (result_type == RESULT_EXIT) {
        return REPL_ACTION_EXIT;
    } else {
        return REPL_ACTION_ERROR;
    }
}

/*!
 * This is the Read-Eval-Print-Loop (aka "REPL") function. We don't actually
 * use `readline` here because the parser is responsible for asking for more
 * input when needed.
 */
void read_eval_print_loop(FILE *input) {

#ifndef NREADLINE
    /* Load command history, if any exists. */
    read_history(SUBPYTHON_HISTORY);

    /* Disable filename auto-complete on TAB key. */
    rl_bind_key('\t', rl_insert);
#endif

    /* Continue consuming input until we are told to exit. */
    while (try_parse(input) != REPL_ACTION_EXIT);

    /* Output a nice exit message. */
    fprintf(stderr, "\nQuitting, goodbye.\n");
}


/*! Prints the program's usage information. */
void usage(FILE *stream, char *program) {
    fprintf(stream,
        "usage: %s [OPTION]... [SCRIPT [ARGS]...]\n"
        "Runs the CS24 Subpython interpreter\n\n"
        " -h             print this help message\n"
        " -m memory_size amount of memory (in bytes) to use for the memory pool\n"
        " -d             run in debug mode:\n"
        "                  the REPL will printing out the current bindings and\n"
        "                  memory contents after every evaluation\n",
        program
    );
}


/*!
 * The entry point to the Subpython interpreter.  Several command-line
 * arguments can be specified, to control various aspects of the program
 * behavior.
 */
int main(int argc, char **argv) {
    size_t memory_size = DEFAULT_MEMORY_SIZE;
    int c;
    while ((c = getopt(argc, argv, "hm:d")) != -1) {
        switch (c) {
            case 'h':
                usage(stdout, argv[0]);
                return 0;

            case 'm':
                memory_size = strtol(optarg, NULL, 10);
                if ((long) memory_size <= 0) {
                    fprintf(stderr, "%s: invalid memory size\n", argv[0]);
                    usage(stderr, argv[0]);
                    return 1;
                }
                break;

            case 'd':
                debug = true;
                break;

            case '?':
                usage(stderr, argv[0]);
                return 1;

            default:
                abort();
        }
    }

    /* If there are remaining arguments, then take the first argument as a
     * script name and ignore any remaining arguments. (In actual Python, these
     * arguments, along with the script name are stored in sys.argv). */
    FILE *input;
    if (optind < argc) {
        char *file = argv[optind];

        input = fopen(file, "r");
        if (input == NULL) {
            fprintf(stderr, "%s: ", argv[0]);
            perror(file);
            return 2;
        }
    }
    else {
        input = stdin;
    }

    interactive = isatty(fileno(input));
    if (errno == EBADF) {
        fprintf(stderr, "%s: can't determine if input is a tty\n", argv[0]);
        return 3;
    }

    if (interactive) {
        printf("CS24 Subpython [Fall 2020]\n");
        printf("Using a memory size of %zu bytes.\n", memory_size);
    }

    /* Allocate the entire memory pool using malloc().
     * This lets us create different memory-pool sizes for testing.
     * Obviously, a real allocator's memory pool would either be a fixed region
     * or would be requested from the operating system (e.g. with sbrk()).
     */
    void *memory_pool = malloc(memory_size);
    if (memory_pool == NULL) {
        fprintf(stderr,
            "%s: could not get %zu bytes from the system\n",
            argv[0], memory_size
        );
        abort();
    }
    refs_init(memory_pool, memory_size);
    eval_init();

    atexit(refs_close);
    atexit(eval_close);

    if (interactive) {
        read_eval_print_loop(input);
        return 0;
    } else {
        return try_parse(input) != REPL_ACTION_CONTINUE;
    }
}

