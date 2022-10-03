/*
 * mm-explicit.c - The best malloc package EVAR!
 *
 * TODO (bug): Uh..this is an implicit list???
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "memlib.h"
#include "mm.h"

/** The required alignment of heap payloads */
const size_t ALIGNMENT = 2 * sizeof(size_t);

/** The layout of each block allocated on the heap */
typedef struct {
    /** The size of the block and whether it is allocated (stored in the low bit) */
    size_t header;
    /**
     * We don't know what the size of the payload will be, so we will
     * declare it as a zero-length array.  This allow us to obtain a
     * pointer to the start of the payload.
     */
    uint8_t payload[];
} block_t;

/** The layout of each block allocated on the heap */
typedef struct {
    /** The size of the block and whether it is allocated (stored in the low bit) */
    size_t header;
    
    block_t *prev;
    block_t *next;
} block_free_t;

typedef struct {
    size_t size;
} footer_t;

static block_free_t *head = NULL;
/** The first and last blocks on the heap */
static block_t *mm_heap_first = NULL;
static block_t *mm_heap_last = NULL;

/** Rounds up `size` to the nearest multiple of `n` */
static size_t round_up(size_t size, size_t n) {
    return (size + (n - 1)) / n * n;
}

/** Set's a block's header with the given size and allocation state */
static void set_header(block_t *block, size_t size, bool is_allocated) {
    block->header = size | is_allocated;
    footer_t *footer = (void *) block + size - sizeof(footer_t);
    footer->size = size;
}

/** Extracts a block's size from its header */
static size_t get_size(block_t *block) {
    return block->header & ~1;
}

/** Extracts a block's allocation state from its header */
static bool is_allocated(block_t *block) {
    return block->header & 1;
}

/**
 * Finds the first free block in the heap with at least the given size.
 * If no block is large enough, returns NULL.
 */
static block_t *find_fit(size_t size) {
    // Traverse the blocks in the heap using the implicit list
    for (block_free_t *curr = head; curr != NULL; curr = (block_free_t *) curr->next) {
        // If the block is free and large enough for the allocation, return it
        if (get_size((block_t *) curr) >= size) {
            return (block_t *) curr;
        }
    }
    return NULL;
}

/** Gets the header corresponding to a given payload pointer */
static block_t *block_from_payload(void *ptr) {
    return ptr - offsetof(block_t, payload);
}

/**
 * mm_init - Initializes the allocator state
 */
bool mm_init(void) {
    // We want the first payload to start at ALIGNMENT bytes from the start of the heap
    void *padding = mem_sbrk(ALIGNMENT - sizeof(block_t));
    if (padding == (void *) -1) {
        return false;
    }

    head = NULL;
    // Initialize the heap with no blocks
    mm_heap_first = NULL;
    mm_heap_last = NULL;
    return true;
}

void add(block_t *block) {
    block_free_t *free = (block_free_t *) block;
    if (head == NULL) {
        head = free;
        head->prev = NULL;
        head->next = NULL;
        return;
    }
    block_free_t *old = head;
    head = free;
    head->prev = NULL;
    head->next = (block_t *) old;
    old->prev = (block_t *) head;
}

void removes(block_t *block) {
    block_free_t *free = (block_free_t *) block;
    block_free_t *prev = (block_free_t *) free->prev;
    block_free_t *next = (block_free_t *) free->next;
    if (free != NULL) {
        if (prev != NULL) {
            prev->next = (block_t *) next;
        } else {
            head = next;
        }
        if (next != NULL) {
            next->prev = (block_t *) prev;
        }
    }
}

/**
 * mm_malloc - Allocates a block with the given size
 */
void *mm_malloc(size_t size) {
    // The block must have enough space for a header and be 16-byte aligned
    size = round_up(sizeof(block_t) + size + sizeof(footer_t), ALIGNMENT);

    // If there is a large enough free block, use it
    block_t *block = find_fit(size);
    if (block != NULL) {
        removes(block);
        block_t *end = (void *) block + size;
        size_t block_size = get_size(block);
        if (get_size(block) > size + sizeof(block_t) + sizeof(footer_t)) {
            if (block == mm_heap_last) {
                mm_heap_last = end;
            }
            set_header(end, get_size(block) - size, false);
            add(end);
            block_size = size;
        }
        set_header(block, block_size, true);
        return block->payload;
    }

    // Otherwise, a new block needs to be allocated at the end of the heap
    block = mem_sbrk(size);
    if (block == (void *) -1) {
        return NULL;
    }

    // Update mm_heap_first and mm_heap_last since we extended the heap
    if (mm_heap_first == NULL) {
        mm_heap_first = block;
    }
    mm_heap_last = block;

    // Initialize the block with the allocated size
    set_header(block, size, true);
    return block->payload;
}

/**
 * mm_free - Releases a block to be reused for future allocations
 */
void mm_free(void *ptr) {
    // mm_free(NULL) does nothing
    if (ptr == NULL) {
        return;
    }

    // Mark the block as unallocated
    block_t *block = block_from_payload(ptr);
    add(block);


    block_t *prev = NULL;
    block_t *next = NULL;
    if (block != mm_heap_first) {
        footer_t *prev_footer = (void *) block - sizeof(footer_t);
        prev = (void *) block - prev_footer->size;
    }
    if (block != mm_heap_last) {
        next = (void *) block + get_size(block);
    }
    
    if (prev != NULL && next != NULL && !is_allocated(prev) && !is_allocated(next)) {
        set_header(prev, get_size(block) + get_size(prev) + get_size(next), false);
        removes(next);
        removes(block);
        if (next == mm_heap_last) {
            mm_heap_last = prev;
        }
    } else if (prev != NULL && !is_allocated(prev)) {
        set_header(prev, get_size(block) + get_size(prev), false);
        removes(block);
        if (block == mm_heap_last) {
            mm_heap_last = prev;
        }
    } else if (next != NULL && !is_allocated(next)) {
        set_header(block, get_size(block) + get_size(next), false);
        removes(next);
        if (next == mm_heap_last) {
            mm_heap_last = block;
        }
    } else {
        set_header(block, get_size(block), false);
    }
}

/**
 * mm_realloc - Change the size of the block by mm_mallocing a new block,
 *      copying its data, and mm_freeing the old block.
 */
void *mm_realloc(void *old_ptr, size_t size) {
    if (old_ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(old_ptr);
        return NULL;
    }
    block_t *old_header = block_from_payload(old_ptr);
    uint8_t *new_ptr = mm_malloc(size);
    size_t cpysize = get_size(old_header) - sizeof(block_t) - sizeof(footer_t);
    if (cpysize > size) {
        cpysize = size;
    }
    memcpy(new_ptr, old_ptr, cpysize);
    mm_free(old_ptr);
    return new_ptr;
}

/**
 * mm_calloc - Allocate the block and set it to zero.
 */
void *mm_calloc(size_t nmemb, size_t size) {
    uint8_t *new_ptr = mm_malloc(size * nmemb);
    block_t *block = block_from_payload(new_ptr);
    memset(block, 0, nmemb);
    return block;
}

/**
 * mm_checkheap - So simple, it doesn't need a checker!
 */
void mm_checkheap(void) {
}
