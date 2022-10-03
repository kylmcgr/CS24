#include "recover_local_secret.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"

const size_t MIN_CHOICE = 'A' - 1;
const size_t MAX_CHOICE = 'Z' + 1;
const size_t SECRET_LENGTH = 5;

static inline page_t *init_pages(void) {
    page_t *pages = calloc(UINT8_MAX + 1, PAGE_SIZE);
    assert(pages != NULL);
    return pages;
}

static inline void flush_all_pages(page_t *pages) {
    for (size_t i = MIN_CHOICE; i < MAX_CHOICE; i++) {
        flush_cache_line(pages[i]);
    }
}

static inline size_t guess_accessed_page(page_t *pages) {
    for (size_t i = MIN_CHOICE; i < MAX_CHOICE; i++) {
        size_t first = time_read(pages[i]);
        size_t second = time_read(pages[i]);
        if (first < 200 && second < 200) {
            return i;
        }
    }
    return 0;
}

static inline void do_access(page_t *pages, size_t secret_index) {
    force_read(pages[access_secret(secret_index)]);
}

int main() {
    page_t *pages = init_pages();

    for (size_t i = 0; i < SECRET_LENGTH; i++) {
        flush_all_pages(pages);
        do_access(pages, i);

        size_t guess = guess_accessed_page(pages);
        if (guess > 0) {
            printf("%c\n", (char) guess);
        } else {
            printf("No page was accessed\n");
        }
    }

    printf("\n");
    free(pages);
}
