#include "consts.h"
#include "cache.h"
#include "handlers.h"

short running = 1;
struct cache_map map = CACHE_MAP_INITIALIZER;

int handle_args(int argc, char *argv[], struct sockaddr_in *my_addr);

int init_listening_socket(int *sockfd, struct sockaddr_in *my_addr);

int init_sigint_handler();

int run_server_handler_thread(struct server_handler_args *args);

void *listen_client_thread(void *arg);

int handle_new_connection(int sockfd);

int main(int argc, char *argv[]) {
    struct sockaddr_in my_addr;
    int listen_socket;

    if (handle_args(argc, argv, &my_addr) < 0 ||
        init_listening_socket(&listen_socket, &my_addr) < 0)
        pthread_exit((void *) EXIT_FAILURE);
    init_sigint_handler();

    while (running) {
        int new_socket;
        new_socket = accept(listen_socket, NULL, NULL);
        if (new_socket == -1) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "Accept failed with: %s\n", strerror(errno));
            break;
        }
        handle_new_connection(new_socket);
    }
    printf("running = %d, exiting\n", running);
    cache_map_destroy(&map);
    close(listen_socket);
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

int init_listening_socket(int *sockfd, struct sockaddr_in *my_addr) {
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == -1) {
        fprintf(stderr, "Error: socket() failed with %s\n", strerror(errno));
        return -1;
    }
    if (bind(listen_socket, (struct sockaddr *) my_addr, sizeof(*my_addr))) {
        fprintf(stderr, "Error: bind() failed with %s\n", strerror(errno));
        return -1;
    }
    if (listen(listen_socket, BACKLOG)) {
        fprintf(stderr, "Error: listen() failed with %s\n", strerror(errno));
        return -1;
    }
    *sockfd = listen_socket;
    return 0;
}

int create_detached_thread(void *(*func)(void *), void *arg) {
    int res;
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    res = pthread_create(&thread, NULL, func, arg);
    pthread_attr_destroy(&attr);
    if (res < 0) {
        fprintf(stderr, "Couldn't create new thread: %s\n", strerror(res));
        return -1;
    }
}

#define RUN_HANDLER(a, b) while (running && res != HANDLER_ERROR) { \
    res = (a);                          \
    if (res == HANDLER_FINISHED) break; \
    if (res == HANDLER_CONTINUE || res == HANDLER_EINTR) continue; \
    fprintf(stderr, (b), strerror(errno));\
}

void *server_thread(void *_arg) {
    struct server_handler_args *arg = (struct server_handler_args *) _arg;
    int res;
    puts("Starting sending request to server");
    RUN_HANDLER(server_handle_out(arg),
            "Error while sending data to server: %s\n");
    puts("Sending request to server finished");
    RUN_HANDLER(server_handle_in(arg),
                "Error while receiving data from server: %s\n");
    puts("Finished receiving data from server");
    destroy_server(arg);
}

void *listen_client_thread(void *arg) {
    int res = 0;
    int sockfd = (int) arg;
    struct client_handler_args args;
    client_handler_args_init(&args, sockfd, run_server_handler_thread, &map);
    RUN_HANDLER(client_handle_in(&args),
            "Error while receiving request from client: %s\n");

    RUN_HANDLER(client_handle_out(&args),
                "Error while sending data to client: %s\n");

    destroy_client(&args);
}

int run_server_handler_thread(struct server_handler_args *args) {
    int res;
    puts("Starting new server thread");
    res = create_detached_thread(server_thread, (void *) args);
    if (res < 0) {
        destroy_server(args);
    }
    return 0;
}


int handle_new_connection(int sockfd) {
    int res;
    printf("Starting handling new connection\n");
    res = create_detached_thread(listen_client_thread, (void *) sockfd);
    if (res < 0) {
        close(sockfd);
    }
}