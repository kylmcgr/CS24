#include "cache_timing.h"

#include <stdio.h>
#include <stdlib.h>

#include "util.h"

const size_t REPEATS = 100000;

int main() {
    uint64_t sum_miss = 0;
    uint64_t sum_hit = 0;

    for (size_t i = 0; i < REPEATS; i++) {
        page_t *pages = malloc(PAGE_SIZE);
        flush_cache_line(pages);
        uint64_t miss = time_read(pages);
        uint64_t hit = time_read(pages);

        if (hit < miss) {
            sum_hit += hit;
            sum_miss += miss;
        }
    }

    printf("average miss = %" PRIu64 "\n", sum_miss / REPEATS);
    printf("average hit  = %" PRIu64 "\n", sum_hit / REPEATS);
}