#include "thread_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct thread_pool {
    pthread_t *thread_pool;
    size_t num_worker_threads;
    queue_t *work;
} thread_pool_t;

typedef struct task {
    work_function_t func;
    void *aux;
} task_t;

void *do_work(void *work_queue){
    queue_t *work = (queue_t *) work_queue;
    while (true) {
        task_t *task = queue_dequeue(work);
        if (task == NULL) {
            return NULL;
        }
        work_function_t func = task->func;
        void *aux = task->aux;
        func(aux);
        free(task);
    }
}

thread_pool_t *thread_pool_init(size_t num_worker_threads) {
    thread_pool_t *pool = malloc(sizeof(thread_pool_t));
    pthread_t *thread_pool = malloc(num_worker_threads * sizeof(pthread_t));
    pool->work = queue_init();

    for (size_t i = 0; i < num_worker_threads; i++) {
        pthread_create(&thread_pool[i], NULL, do_work, pool->work);
    }

    pool->thread_pool = thread_pool;
    pool->num_worker_threads = num_worker_threads;
    return pool;
}

void thread_pool_add_work(thread_pool_t *pool, work_function_t function, void *aux) {
    task_t *task = malloc(sizeof(task_t));
    task->func = function;
    task->aux = aux;
    queue_enqueue(pool->work, task);
}

void thread_pool_finish(thread_pool_t *pool) {
    for (size_t i = 0; i < pool->num_worker_threads; i++) {
        queue_enqueue(pool->work, NULL);
    }
    for (size_t i = 0; i < pool->num_worker_threads; i++) {
        pthread_join(pool->thread_pool[i], NULL);
    }

    queue_free(pool->work);
    free(pool->thread_pool);
    free(pool);
}