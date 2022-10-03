#define _GNU_SOURCE 1

#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "util.h"

static const size_t PAGE_SIZE = 4096;
typedef uint8_t page_t[PAGE_SIZE];

static void *const START_PAGE = (void *) ((size_t) 1 << 32);
static const size_t MAX_HEAP_SIZE = (size_t) 1 << 30;
static const int HEAP_MMAP_FLAGS = MAP_ANONYMOUS | MAP_PRIVATE;
static const size_t HEADER_MAGIC = 0x0123456789ABCDEF;

typedef struct {
    size_t magic;
    size_t size;
    bool is_allocated;
} header_t;

static bool is_initialized = false;
static page_t *current_page;

static size_t pages_round_up(size_t size) {
    return (size + PAGE_SIZE - 1) / PAGE_SIZE;
}

static void set_header(page_t *header_page, size_t size, bool is_allocated) {
    mprotect((void *) header_page, PAGE_SIZE, PROT_READ | PROT_WRITE);
    header_t *header = (header_t *) header_page;
    header->magic = HEADER_MAGIC;
    header->size = size;
    header->is_allocated = is_allocated;
    mprotect((void *) header_page, PAGE_SIZE, PROT_NONE);
}

static void *get_payload(page_t *header_page, size_t size) {
    void *payload = (void *) header_page + (pages_round_up(size) + 1) * PAGE_SIZE - size;
    return payload;
}

static void check_for_leaks(void) {
    // Prevent memory leaks from stdout
    fclose(stdout);
    for (header_t *curr = START_PAGE; (void *) curr < START_PAGE + MAX_HEAP_SIZE; curr = (void *) curr + PAGE_SIZE) {
        mprotect((void *) curr, PAGE_SIZE, PROT_READ | PROT_WRITE);
        if (curr->magic == HEADER_MAGIC && curr->is_allocated) {
            report_memory_leak(get_payload((page_t *) curr, curr->size), curr->size);
        }
        mprotect((void *) curr, PAGE_SIZE, PROT_NONE);
    }
}

static void sigsegv_handler(int signum, siginfo_t *siginfo, void *context) {
    void *address = siginfo->si_addr;
    if (address >= START_PAGE && address < (void *) (current_page + 1)) {
        report_invalid_heap_access(address);
    }
    report_seg_fault(address);
    (void) signum;
    (void) context;
}

static void asan_init(void) {
    if (is_initialized) {
        return;
    }

    struct sigaction act = {.sa_sigaction = sigsegv_handler, .sa_flags = SA_SIGINFO};
    sigaction(SIGSEGV, &act, NULL);

    // Avoid buffering on stdout
    setbuf(stdout, NULL);

    current_page = mmap(START_PAGE, MAX_HEAP_SIZE,
                        PROT_NONE,
                        HEAP_MMAP_FLAGS, -1, 0);
    assert(current_page == START_PAGE);

    atexit(check_for_leaks);

    is_initialized = true;
}

void *malloc(size_t size) {
    asan_init();

    size_t pages_necessary = pages_round_up(size);

    // Store the size of the allocation at the beginning of the page before the payload
    page_t *header_page = current_page;

    set_header(header_page, size, true);
    current_page += 1 + pages_necessary;

    mprotect((void *) header_page + PAGE_SIZE, pages_round_up(size) * PAGE_SIZE, PROT_READ | PROT_WRITE);
    
    // Provide the user with the END of the first page
    return get_payload(header_page, size);
}

void free(void *ptr) {
    asan_init();

    if (ptr == NULL) {
        return;
    }

    if (ptr < START_PAGE + PAGE_SIZE || ptr >= START_PAGE + MAX_HEAP_SIZE) {
        report_invalid_free(ptr);
    }
    header_t *header = ptr - ((uintptr_t) ptr % PAGE_SIZE) - PAGE_SIZE;
    mprotect((void *) header, PAGE_SIZE, PROT_READ | PROT_WRITE);
    if (header->magic != HEADER_MAGIC || ptr != get_payload((page_t *) header, header->size)) {
        report_invalid_free(ptr);
    }
    if (!header->is_allocated) {
        report_double_free(ptr, header->size);
    }
    header->is_allocated = false;
    mprotect((void *) header + PAGE_SIZE, pages_round_up(header->size) * PAGE_SIZE, PROT_NONE);
    mprotect((void *) header, PAGE_SIZE, PROT_NONE);
}
