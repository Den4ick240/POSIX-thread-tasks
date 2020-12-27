#include "consts.h"
#if defined(THREADPOOL) || defined(SINGLETHREAD)
#include "cache.h"
#include "handlers.h"
#include "arrayset.h"
#include "threadpool.h"
#include "pollfdset.h"

struct client {
    struct client_handler_args args;
    struct pollfd *pollfd;
};

struct server {
    struct server_handler_args *args;
    struct pollfd *pollfd;
};

struct cache_map map = CACHE_MAP_INITIALIZER;
struct arrayset clients = ARRAY_SET_INITIALIZER,
        servers = ARRAY_SET_INITIALIZER;
struct pollfdset pollfdset;
struct thread_pool thread_pool;
#ifdef THREADPOOL
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t semaphore;
#endif

short running = 1;

int handle_args(int argc, char *argv[], struct sockaddr_in *my_addr);

int init_listening_pollfd(struct pollfd *pollfd, struct sockaddr_in *my_addr);

int init_sigint_handler();

int create_server_connection(struct server_handler_args *args) {
    struct server *server = (struct server *) malloc(sizeof(struct server));
    if (server == NULL) return -1;
    server->pollfd = allocate_pollfd(&pollfdset, args->socket, POLLIN | POLLOUT);
    if (server->pollfd == NULL) {
        free(server);
        return -1;
    }
    server->args = args;
#ifdef THREADPOOL
    pthread_mutex_lock(&server_mutex);
#endif
    arrayset_add(&servers, server);
#ifdef THREADPOOL
    pthread_mutex_unlock(&server_mutex);
#endif
    return 0;
}

void handle_accept(void *arg) {
    struct pollfd *pollfd = (struct pollfd *) arg;
    int new_socket;
    struct pollfd *client_pollfd;
    struct client *client;
    puts("handling accept");
    if (pollfd->revents != POLLIN) {
        printf("Unexpected events on listening pollfd: %d\n", pollfd->revents);
        running = 0;
#ifdef THREADPOOL
        sem_post(&semaphore);
#endif
        return;
    }
    new_socket = accept(pollfd->fd, NULL, NULL);
    puts("accepted");
    if (new_socket < 0) {
        fprintf(stderr, "Accept failed: %s\n");
    }
    client_pollfd = allocate_pollfd(&pollfdset, new_socket, POLLIN);
    if (client_pollfd == NULL) {
        puts("Couldn't allocate pollfd for client, closing connection");
        close(new_socket);
#ifdef THREADPOOL
        sem_post(&semaphore);
#endif
        return;
    }
//    puts("allocated");
#ifdef THREADPOOL
    pthread_mutex_lock(&client_mutex);
#endif
//    puts("locked");
    client = (struct client *) malloc(sizeof(struct client));
    if (client == NULL) {
        free_pollfd(&pollfdset, client_pollfd);
        close(new_socket);
    } else {
        client->pollfd = client_pollfd;
        client_handler_args_init(&client->args, new_socket, create_server_connection, &map);
        arrayset_add(&clients, client);
    }
#ifdef THREADPOOL
    pthread_mutex_unlock(&client_mutex);
    sem_post(&semaphore);
#endif
}

void remove_client(struct client *client) {
    puts("removing client");
#ifdef THREADPOOL
    pthread_mutex_lock(&client_mutex);
#endif
    arrayset_remove(&clients, client);
    free_pollfd(&pollfdset, client->pollfd);
    destroy_client(&client->args);
#ifdef THREADPOOL
    pthread_mutex_unlock(&client_mutex);
#endif
}

void remove_server(struct server *server) {
//    puts("removing server");
#ifdef THREADPOOL
    pthread_mutex_lock(&server_mutex);
#endif
    arrayset_remove(&servers, server);
    free_pollfd(&pollfdset, server->pollfd);
    destroy_server(server->args);
#ifdef THREADPOOL
    pthread_mutex_unlock(&server_mutex);
#endif
}

void handle_client(void *arg) {
    int res1 = HANDLER_CONTINUE, res2 = HANDLER_CONTINUE;
    struct client *client = (struct client *) arg;
//    puts("Handling client");
    if (client->pollfd->revents & (POLLHUP | POLLERR)) {
        perror("Error or hang up on client socket");
        remove_client(client);
#ifdef THREADPOOL
        sem_post(&semaphore);
#endif
        return;
    }

//    printf("SDLKSFJL:SKDJFL:SD  POLLIN %d\n", client->pollfd->revents & POLLIN);
    if (client->pollfd->revents & POLLIN && running) {
//        puts("Handling in");
        while ((res1 = client_handle_in(&client->args)) == HANDLER_EINTR && running);
        if (res1 != HANDLER_CONTINUE) {
            client->pollfd->events &= ~POLLIN;
            client->pollfd->events |= POLLOUT;
            printf("Finished receiving request from client: %d\n", res1);
//            sleep(1);
        }
    }

//    printf("SDLKSFJL:SKDJFL:SD  POLLOUT %d\n", client->pollfd->revents & POLLOUT);
    if (client->pollfd->revents & POLLOUT && running && res1 != HANDLER_ERROR) {
//        puts("client handling out");
        while ((res2 = client_handle_out(&client->args)) == HANDLER_EINTR && running);
//        printf("Client handled out %d\n", res2);
        if (res2 != HANDLER_CONTINUE) {
            client->pollfd->events &= ~POLLOUT;
            printf("Finished sending data to client: %d\n", res2);
        }
    }
    if (res1 == HANDLER_ERROR || res2 == HANDLER_ERROR || res2 == HANDLER_FINISHED || !running) {
        remove_client(client);
    }
#ifdef THREADPOOL
    sem_post(&semaphore);
#endif
}

void handle_server(void *arg) {
    int res1 = HANDLER_CONTINUE, res2 = HANDLER_CONTINUE;
    struct server *server = (struct server *) arg;
//    puts("Handling server");
    if (server->pollfd->revents & (POLLHUP | POLLERR)) {
        perror("Error or hang up on server socket");
        remove_server(server);
#ifdef THREADPOOL
        sem_post(&semaphore);
#endif
        return;
    }

//    printf("server-----------------  POLLIN %d\n", server->pollfd->revents & POLLIN);
    if (server->pollfd->revents & POLLIN && running) {
        while ((res1 = server_handle_in(server->args)) == HANDLER_EINTR && running);
//        printf("Server handled in %d\n", res1);
//        sleep(1);
        if (res1 != HANDLER_CONTINUE) {
            server->pollfd->events &= ~POLLIN;
            printf("Finished receiving daata from server: %d\n", res1);
        }
    }
//    printf("server-----------------  POLLOUT %d\n", server->pollfd->revents & POLLOUT);
    if (server->pollfd->revents & POLLOUT && running && res1 != HANDLER_ERROR) {
        while ((res2 = server_handle_out(server->args)) == HANDLER_EINTR && running);
//        printf("Server handled out %d\n", res2);
//        sleep(1);
        if (res2 != HANDLER_CONTINUE) {
            server->pollfd->events &= ~POLLOUT;
            printf("Finished sending request to server:%d\n", res2);
        }
    }
    if (res1 == HANDLER_ERROR || res2 == HANDLER_ERROR || res1 == HANDLER_FINISHED || !running) {
        remove_server(server);
    }
#ifdef THREADPOOL
    sem_post(&semaphore);
#endif
}

#define ADD_TASK_TO_SCHEDULE(task, arg)                       \
if (thread_pool_add_task(&thread_pool, (task), (arg)) != 0) {  \
    printf("thread_pool_add_task() failed.\n");                 \
    running = 0;                                                 \
    return; }

void poll_task(void *arg) {
//    puts("Polling");
    struct pollfd *listening_pollfd = (struct pollfd *) arg;
    int pollret = poll(pollfdset.fds, pollfdset.max_occupied_fd, POLL_TIMEOUT);
    int i, task_cnt = 0, res;

//    printf("Poll : %d\n", pollret);
    if (pollret < 0) {
        perror("Pollret: ");
    }

    if (task_cnt < pollret && listening_pollfd->revents != 0) {
        task_cnt++;
        ADD_TASK_TO_SCHEDULE(handle_accept, (void *) listening_pollfd);
    }
#ifdef THREADPOOL
    pthread_mutex_lock(&client_mutex);
#endif
    for (i = 0; i < clients.data_size && task_cnt < pollret; i++) {
        struct client *client = (struct client *) clients.arr[i];
//        puts("Checking client");
        if (client->pollfd->revents != 0) {
//            puts("starting client");
            task_cnt++;
            ADD_TASK_TO_SCHEDULE(handle_client, (void *) client);
        }
    }
#ifdef THREADPOOL
    pthread_mutex_unlock(&client_mutex);
    pthread_mutex_lock(&server_mutex);
#endif
    for (i = 0; i < servers.data_size && task_cnt < pollret; i++) {
        struct server *server = (struct server *) servers.arr[i];
        if (server->pollfd->revents != 0) {
            task_cnt++;
            ADD_TASK_TO_SCHEDULE(handle_server, (void *) server);
        }
    }
#ifdef THREADPOOL
    pthread_mutex_unlock(&server_mutex);
//    puts("poll task waiting other tasks");
    for (i = 0; i < task_cnt; i++) {
        while (sem_wait(&semaphore) == -1 && errno == EINTR);
    }
//    puts("poll task done waiting other tasks");
#endif
    if (running) {
        ADD_TASK_TO_SCHEDULE(poll_task, arg);
    } else {
        thread_pool_shut_down(&thread_pool, 0);
    }
}

void free_client(void *arg) {
    struct client *client = (struct client *) arg;
    destroy_client(&client->args);
    free_pollfd(&pollfdset, client->pollfd);
    free(arg);
}

void free_server(void *arg) {
    struct server *server = (struct server *) arg;
    destroy_server(server->args);
    free_pollfd(&pollfdset, server->pollfd);
    free(server);
}

int main(int argc, char *argv[]) {
    struct sockaddr_in my_addr;
    struct pollfd *listen_pollfd;
//    struct thread_pool *thread_pool;
    pollfdset_init(&pollfdset);
//    puts("pksdkl;fjsdkl'fgdgdsgsdfgsdflkughsd;ofhg");
    listen_pollfd = allocate_pollfd(&pollfdset, 0, POLLIN);

    if (handle_args(argc, argv, &my_addr) < 0 ||
        init_listening_pollfd(listen_pollfd, &my_addr) < 0)
        pthread_exit((void *) EXIT_FAILURE);
//    puts("Inited lsd");
//    init_sigint_handler();
#ifdef THREADPOOL
    sem_init(&semaphore, 0, 0);
#endif
//    puts("Initializing thread pool");
    thread_pool_init(&thread_pool, THREAD_NUM);
//    puts("Thr");

    if (thread_pool_add_task(&thread_pool, poll_task, (void *) listen_pollfd) == 0) {
#ifdef SINGLETHREAD
        thread_pool_run(&thread_pool);
#endif
    }
//    puts("Task added");
    thread_pool_destroy(&thread_pool);
    arrayset_free(&clients, free_client);
    arrayset_free(&servers, free_server);
    cache_map_destroy(&map);
    if (close(listen_pollfd->fd)) {
        perror("Couldn't close listening socket: ");
    } else {
        puts("Listening socket closed");
    }
    pthread_exit((void *) NULL);
}

void sigint_handler(int signum) { running = 0; }

int init_sigint_handler() {
    signal(SIGINT, sigint_handler);
    return 0;
}

int handle_args(int argc, char *argv[], struct sockaddr_in *my_addr) {
    int listen_port;
    if (argc == 1) {
        fprintf(stderr, "Usage: %s listen_port\n", argv[0]);
        return -1;
    }
    listen_port = atoi(argv[1]);
    if (listen_port <= 0) {
        fprintf(stderr, "listen_port should be a valid port\n");
        return -1;
    }
    memset(my_addr, 0, sizeof(struct sockaddr_in));
    my_addr->sin_family = AF_INET;
    my_addr->sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr->sin_port = htons(listen_port);
    return 0;
}

int init_listening_pollfd(struct pollfd *pollfd, struct sockaddr_in *my_addr) {
    int enable = 1;
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == -1) {
        fprintf(stderr, "Error: socket() failed with %s\n", strerror(errno));
        return -1;
    }
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
    }
    if (bind(listen_socket, (struct sockaddr *) my_addr, sizeof(*my_addr))) {
        fprintf(stderr, "Error: bind() failed with %s\n", strerror(errno));
        return -1;
    }
    if (listen(listen_socket, BACKLOG)) {
        fprintf(stderr, "Error: listen() failed with %s\n", strerror(errno));
        return -1;
    }
    pollfd->fd = listen_socket;
    pollfd->events = POLLIN;
    pollfd->revents = 0;
    return 0;
}
#endif