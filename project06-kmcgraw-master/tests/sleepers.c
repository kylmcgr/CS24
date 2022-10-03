#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "thread_pool.h"

const size_t NUM_SLEEPS = 10;

void usage(char *argv[]) {
    fprintf(stderr, "Usage: %s <# of threads>\n", argv[0]);
    exit(1);
}

void sleeper(void *aux) {
    (void) aux;
    sleep(1);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        usage(argv);
    }

    size_t num_threads = strtoul(argv[1], NULL, 0);
    if (num_threads == 0) {
        usage(argv);
    }

    thread_pool_t *pool = thread_pool_init(num_threads);

    for (size_t i = 0; i < NUM_SLEEPS; i++) {
        thread_pool_add_work(pool, sleeper, NULL);
    }

    thread_pool_finish(pool);
}
