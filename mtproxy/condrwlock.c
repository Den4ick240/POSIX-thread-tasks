#include "condrwlock.h"

#define return_if_error(x) if ((res = (x)) != 0) { cond_rwlock_destroy(cond_rwlock); return res; }
#define return_if_dropped(x) if (cond_rwlock->dropped) return (x)

//#define lockdebug printf("locked by thread %d\n", pthread_self());
//#define unlockdebug printf("unlocked by thread %d\n", pthread_self());
//#define waitdebug printf("wait by thread %d\n", pthread_self());
//#define wakedebug printf("wake by thread %d\n", pthread_self());

#define lockdebug ;
#define unlockdebug ;
#define waitdebug ;
#define wakedebug ;



int cond_rwlock_init(struct cond_rwlock *cond_rwlock) {
    int res;
    return_if_error(pthread_mutex_init(&cond_rwlock->mutex, NULL));
    return_if_error(pthread_cond_init(&cond_rwlock->rd_cond, NULL));
    return_if_error(pthread_cond_init(&cond_rwlock->wr_cond, NULL));
    cond_rwlock->readers = 0;
    cond_rwlock->dropped = 0;
    return 0;
}

int cond_rwlock_rdlock(struct cond_rwlock *cond_rwlock) {
    return cond_rwlock_wait_and_rdlock(cond_rwlock, NULL, NULL);
}

int cond_rwlock_wait_and_rdlock(struct cond_rwlock *cond_rwlock, int (*predicate)(void *), void *predicate_args) {
    return_if_dropped(0);
    pthread_mutex_lock(&cond_rwlock->mutex);lockdebug
    while (predicate != NULL && !predicate(predicate_args) && !cond_rwlock->dropped) {
        waitdebug
        pthread_cond_wait(&cond_rwlock->rd_cond, &cond_rwlock->mutex);
        wakedebug
    }
    if (cond_rwlock->dropped) {
        pthread_mutex_unlock(&cond_rwlock->mutex);unlockdebug
        return 1;
    }
    (cond_rwlock->readers)++;
    pthread_mutex_unlock(&cond_rwlock->mutex);unlockdebug
    return 0;
}

int cond_rwlock_rdunlock(struct cond_rwlock *cond_rwlock) {
    return_if_dropped(0);
    pthread_mutex_lock(&cond_rwlock->mutex);lockdebug

    if (!cond_rwlock->dropped && --(cond_rwlock->readers) == 0) {
        pthread_cond_broadcast(&cond_rwlock->wr_cond);
    }
    pthread_mutex_unlock(&cond_rwlock->mutex);unlockdebug
    return 0;
}

int cond_rwlock_wrlock(struct cond_rwlock *cond_rwlock) {
    return_if_dropped(1);
    pthread_mutex_lock(&cond_rwlock->mutex);lockdebug

    while (cond_rwlock->readers != 0 && !cond_rwlock->dropped) {
        waitdebug
        pthread_cond_wait(&cond_rwlock->wr_cond, &cond_rwlock->mutex);
        wakedebug
    }
    if (cond_rwlock->dropped) {
        pthread_mutex_unlock(&cond_rwlock->mutex);unlockdebug
        return -1;
    }
    return 0;
}

int cond_rwlock_wrunlock(struct cond_rwlock *cond_rwlock) {
//    return_if_dropped(1);
    pthread_cond_broadcast(&cond_rwlock->rd_cond);
    pthread_mutex_unlock(&cond_rwlock->mutex);unlockdebug
    return 0;
}

int cond_rwlock_drop(struct cond_rwlock *cond_rwlock) {
    return_if_dropped(1);
    pthread_mutex_lock(&cond_rwlock->mutex);lockdebug

    cond_rwlock->dropped = 1;
    pthread_cond_broadcast(&cond_rwlock->rd_cond);
    pthread_cond_broadcast(&cond_rwlock->wr_cond);
    pthread_mutex_unlock(&cond_rwlock->mutex);unlockdebug
    return 0;
}

int cond_rwlock_destroy(struct cond_rwlock *cond_rwlock) {
    int res1 = pthread_mutex_destroy(&cond_rwlock->mutex);
    if (res1) fprintf(stderr, "couldn't destroy mutex of rwlock: %s\n", strerror(res1));
    int res2 = pthread_cond_destroy(&cond_rwlock->rd_cond);
    if (res2) perror(strerror(res1));
    int res3 = pthread_cond_destroy(&cond_rwlock->wr_cond);
    if (res3) perror(strerror(res1));
    if (res1 || res2 || res3)
        return -1;
    else
        return 0;
}












































