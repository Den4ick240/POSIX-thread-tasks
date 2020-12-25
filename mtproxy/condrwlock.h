#ifndef COND_RWLOCK
#define COND_RWLOCK
#include "consts.h"

#define COND_RWLOCK_INITIALIZER { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0 };

struct cond_rwlock {
    pthread_mutex_t mutex;
    pthread_cond_t rd_cond, wr_cond;
    int readers, dropped;
};

int cond_rwlock_init(struct cond_rwlock *cond_rwlock);
int cond_rwlock_wait_and_rdlock(struct cond_rwlock *cond_rwlock, int (*predicate)(void*), void *predicate_args);
int cond_rwlock_rdlock(struct cond_rwlock *cond_rwlock);
int cond_rwlock_rdunlock(struct cond_rwlock *cond_rwlock);
int cond_rwlock_wrlock(struct cond_rwlock *cond_rwlock);
int cond_rwlock_wrunlock(struct cond_rwlock *cond_rwlock);
int cond_rwlock_drop(struct cond_rwlock *cond_rwlock);
int cond_rwlock_destroy(struct cond_rwlock *cond_rwlock);

#endif //COND_RWLOCK