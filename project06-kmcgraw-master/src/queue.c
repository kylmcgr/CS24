#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct node {
    void *value;
    void *next;
} node_t;

typedef struct queue {
    node_t *head;
    node_t *tail;
    pthread_mutex_t lock;
    pthread_cond_t cv;
} queue_t;

queue_t *queue_init(void) {
    queue_t *queue = malloc(sizeof(queue_t));
    if (queue == NULL) {
        fprintf(stderr, "Fatal error: out of memory. Terminating program.\n");
        exit(1);
    }
    queue->head = NULL;
    queue->tail = NULL;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cv, NULL);
    return queue;
}

void queue_enqueue(queue_t *queue, void *value) {
    pthread_mutex_lock(&queue->lock);
    node_t *new = malloc(sizeof(node_t));
    if (new == NULL) {
        fprintf(stderr, "Fatal error: out of memory. Terminating program.\n");
        exit(1);
    }
    new->value = value;
    new->next = NULL;
    if (queue->head == NULL) {
        queue->head = new;
    } else {
        queue->tail->next = new;
    }
    queue->tail = new;
    pthread_mutex_unlock(&queue->lock);
    pthread_cond_signal(&queue->cv);
    return;
}

void *queue_dequeue(queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    while (queue->head == NULL) {
        pthread_cond_wait(&queue->cv, &queue->lock);
    }
    node_t *old = queue->head;
    void *value = old->value;
    queue->head = old->next;
    free(old);
    pthread_mutex_unlock(&queue->lock);
    return value;
}

void queue_free(queue_t *queue) {
    if (queue->head != NULL) {
        return;
    }
    free(queue);
}