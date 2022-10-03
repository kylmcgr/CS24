#define _GNU_SOURCE
#include <assert.h>
#include <crypt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dictionary_words.h"
#include "thread_pool.h"

const char HASH_START[] = "$6$";
const size_t SALT_LENGTH = 20;
const size_t HASH_LENGTH = 106;
const size_t NUM_THREADS = 16;

static size_t hash_count = 0;
static char **hashes = NULL;

static inline bool hashes_match(const char *password, const char *hash) {
    char salt[SALT_LENGTH + 1];
    memcpy(salt, hash, sizeof(char[SALT_LENGTH]));
    salt[SALT_LENGTH] = '\0';
    struct crypt_data data;
    memset(&data, 0, sizeof(data));
    char *hashed = crypt_r(password, salt, &data);
    return strcmp(&hashed[SALT_LENGTH], &hash[SALT_LENGTH]) == 0;
}

void check_word(void *_n) {
    char *word = (char *) _n;
    for (size_t i = 0; i < 10; i++) {
        for (size_t j = 0; j <= strlen(word); j++) {
            char *str = (char*)malloc(strlen(word)+1);
            char *num = (char*)malloc(1);
            memset(str, 0, strlen(word)+1);
            strncpy(str, word, j);
            sprintf(num, "%zu", i);
            strcat(str, num);
            strcat(str, word + j);
            for (size_t k = 0; k < hash_count; k++){
                if (hashes_match(str, hashes[k])) {
                    printf("%s\n", str);
                }
            }
            free(str);
            free(num);
        }
    }
}

int main(void) {
    // Read in the hashes from the standard input
    char *line = NULL;
    size_t line_capacity = 0;
    while (getline(&line, &line_capacity, stdin) > 0 && line[0] != '\n') {
        hashes = realloc(hashes, sizeof(char * [hash_count + 1]));
        assert(hashes != NULL);
        assert(strlen(line) == HASH_LENGTH + 1 &&
               strncmp(line, HASH_START, strlen(HASH_START)) == 0 &&
               line[HASH_LENGTH] == '\n');
        char *hash = malloc(sizeof(char[HASH_LENGTH + 1]));
        assert(hash != NULL);
        memcpy(hash, line, sizeof(char[HASH_LENGTH]));
        hash[HASH_LENGTH] = '\0';
        hashes[hash_count++] = hash;
    }
    free(line);

    thread_pool_t *pool = thread_pool_init(NUM_THREADS);

    for (size_t i = 0; i < NUM_DICTIONARY_WORDS; i++) {
        thread_pool_add_work(pool, check_word, DICTIONARY[i]);
    }

    thread_pool_finish(pool);
}
