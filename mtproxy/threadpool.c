#include "threadpool.h"

struct thread_pool_task {
    void (*task)(void *);

    void *args;
};

void *thread_method(void *arg) {
    struct thread_pool *thread_pool = (struct thread_pool *) arg;
#ifdef THREADPOOL
    for (;;)
#else
        while (!queue_is_empty(&thread_pool->task_queue))
#endif
    {
        struct thread_pool_task *task_t;
#ifdef THREADPOOL
        pthread_mutex_lock(&thread_pool->queue_mutex);
        while (queue_is_empty(&thread_pool->task_queue) && !thread_pool->shut_down) {
//            printf("thread %d waiting\n", pthread_self());
            pthread_cond_wait(&thread_pool->queue_cond, &thread_pool->queue_mutex);
        }
        if (queue_is_empty(&thread_pool->task_queue) && thread_pool->shut_down) {
            pthread_mutex_unlock(&thread_pool->queue_mutex);
            pthread_exit(NULL);
        }
#endif
        task_t = queue_pop(&(thread_pool->task_queue));
#ifdef THREADPOOL
        pthread_mutex_unlock(&thread_pool->queue_mutex);
#endif
//        printf("Thread %d started task\n", pthread_self());
        task_t->task(task_t->args);
//        printf("Thread %d finished task\n", pthread_self());
        free(task_t);
    }
}

int thread_pool_init(struct thread_pool *thread_pool, int thread_num) {
    int i, res;
    thread_pool->shut_down = 0;
    queue_init(&thread_pool->task_queue);
#ifdef THREADPOOL
    thread_pool->thread_num = thread_num;
    thread_pool->threads = (pthread_t *) malloc(sizeof(pthread_t) * thread_num);
    if (thread_pool->threads == NULL) {
        return -1;
    }
    if ((res = pthread_mutex_init(&thread_pool->queue_mutex, NULL)) != 0) {
        return res;
    }
    if ((res = pthread_cond_init(&thread_pool->queue_cond, NULL)) != 0) {
        return res;
    }

    for (i = 0; i < thread_num; i++) {
        if ((res = pthread_create(thread_pool->threads + i, NULL, thread_method, thread_pool)) != 0) {
            thread_pool->thread_num = i;
            thread_pool_shut_down(thread_pool, 1);
            thread_pool_destroy(thread_pool);
            return res;
        }
    }
#endif
    return 0;
}


int thread_pool_add_task(struct thread_pool *thread_pool, void (*task)(void *), void *args) {
//    puts("added task");
    int res;
    struct thread_pool_task *task_s = (struct thread_pool_task *) malloc(sizeof(struct thread_pool_task));
    if (task_s == NULL) {
        return -1;
    }
    task_s->task = task;
    task_s->args = args;

#ifdef THREADPOOL
    pthread_mutex_lock(&thread_pool->queue_mutex);
#endif
    res = queue_add(&thread_pool->task_queue, task_s);
    if (res != 0) {
#ifdef THREADPOOL
        pthread_mutex_unlock(&thread_pool->queue_mutex);
#endif
        return -1;
    }

#ifdef THREADPOOL
    pthread_cond_signal(&thread_pool->queue_cond);
    pthread_mutex_unlock(&thread_pool->queue_mutex);
#endif

    return 0;
}

void thread_pool_run(struct thread_pool *thread_pool) {
    thread_method((void *) thread_pool);
}

void clear_task(void *task) {
    free(task);
}

int thread_pool_shut_down(struct thread_pool *thread_pool, int clear_queue) {
    puts("Shuting down");
#ifdef THREADPOOL
    pthread_mutex_lock(&thread_pool->queue_mutex);
#endif
    thread_pool->shut_down = 1;
    if (clear_queue) {
        queue_clear(&thread_pool->task_queue, clear_task);
    }
#ifdef THREADPOOL
    pthread_cond_broadcast(&thread_pool->queue_cond);
    pthread_mutex_unlock(&thread_pool->queue_mutex);
#endif
    return 0;
}

int thread_pool_destroy(struct thread_pool *thread_pool) {
#ifdef THREADPOOL
    int i;
    for (i = 0; i < thread_pool->thread_num; i++) {
        puts("Joining thread");
        pthread_join(thread_pool->threads[i], NULL);
        puts("Thread joined");
    }
    free(thread_pool->threads);
    pthread_cond_destroy(&thread_pool->queue_cond);
    pthread_mutex_destroy(&thread_pool->queue_mutex);
#endif
    return 0;
}
