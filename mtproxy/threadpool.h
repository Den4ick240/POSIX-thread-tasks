#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "consts.h"
#include "queue.h"

struct thread_pool {
    struct queue task_queue;
    int shut_down;
#ifdef THREADPOOL
    int thread_num;
    pthread_t *threads;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
#endif
};

int thread_pool_init(struct thread_pool *thread_pool, int thread_num);
int thread_pool_add_task(struct thread_pool *thread_pool, void (*task) (void*), void *args);
void thread_pool_run(struct thread_pool *thread_pool);
int thread_pool_shut_down(struct thread_pool *thread_pool, int clear_queue);
int thread_pool_destroy(struct thread_pool *thread_pool);

#endif //THREAD_POOL_H