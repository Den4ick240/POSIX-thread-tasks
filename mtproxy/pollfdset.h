#ifndef POLLFD_SET
#define POLLFD_SET

#include "consts.h"

#ifndef MAX_POLLFD_NUM
#define MAX_POLLFD_NUM 1020
#endif //MAX_POLLFD_NUM

#define UNOCCUPIED_DESCRIPTOR -1
#define OCCUPIED_DESCRIPTOR -2

struct pollfdset {
    struct pollfd fds[MAX_POLLFD_NUM];
    int max_occupied_fd;
#ifdef THREADPOOL
    pthread_mutex_t mutex;
#endif
};

void init_pollfd(struct pollfd *pollfd);

int pollfdset_init(struct pollfdset *set);

struct pollfd *allocate_pollfd(struct pollfdset *set, int fd, int events);

void pollfdset_trim(struct pollfdset *set);

void free_pollfd(struct pollfdset *set, struct pollfd *pollfd);

void pollfdset_destroy(struct pollfdset *set);
#endif //POLLFD_SET