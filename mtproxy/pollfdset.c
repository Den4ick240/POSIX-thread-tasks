#include "pollfdset.h"

void init_pollfd(struct pollfd *pollfd) {
    pollfd->fd = UNOCCUPIED_DESCRIPTOR;
    pollfd->events = 0;
    pollfd->revents = 0;
}

int pollfdset_init(struct pollfdset *set) {
    int i;
#ifdef THREADPOOL
    int res = pthread_mutex_init(&set->mutex, NULL);
    if (res != 0) {
        return -1;
    }
#endif
    for (i = 0; i < MAX_POLLFD_NUM; i++)
        init_pollfd(set->fds + i);
    set->max_occupied_fd = 0;
    return 0;
}

struct pollfd *allocate_pollfd(struct pollfdset *set, int fd, int events) {
    int i;
#ifdef THREADPOOL
    pthread_mutex_lock(&set->mutex);
#endif
    for (i = 0; i < MAX_POLLFD_NUM; i++) {
        if (i == set->max_occupied_fd)
            set->max_occupied_fd = i + 1;
        if (set->fds[i].fd == UNOCCUPIED_DESCRIPTOR) {
            set->fds[i].fd = fd;
            set->fds[i].events = events;
#ifdef THREADPOOL
            pthread_mutex_unlock(&set->mutex);
#endif
            return &set->fds[i];
        }
    }
#ifdef THREADPOOL
    pthread_mutex_unlock(&set->mutex);
#endif
    return NULL;
}

void pollfdset_trim(struct pollfdset *set) {
    for (; set->max_occupied_fd > 0; set->max_occupied_fd--)
        if (set->fds[set->max_occupied_fd - 1].fd != UNOCCUPIED_DESCRIPTOR)
            return;
}

void free_pollfd(struct pollfdset *set, struct pollfd *pollfd) {
#ifdef THREADPOOL
    pthread_mutex_lock(&set->mutex);
#endif
    init_pollfd(pollfd);
    pollfdset_trim(set);
#ifdef THREADPOOL
    pthread_mutex_unlock(&set->mutex);
#endif
}

void pollfdset_destroy(struct pollfdset *set) {
#ifdef THREADPOOL
    pthread_mutex_destroy(&set->mutex);
#endif
}
