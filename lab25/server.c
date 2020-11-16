/*
 * Lab25 OS
 * TSP connection translator
 */
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#define FDS_NUM 1024
#define MAX_CONNECTION_NUM 510
#define BACKLOG MAX_CONNECTION_NUM
#define TIMEOUT 0
#define BUFFER_SIZE 4096
#define CLOSED_CONNECTION -1

struct node {
    struct node *next, *prev;
    char line[BUFFER_SIZE];
    int len;
    int handled_bytes;
};

struct cache {
    struct node *first, *last;
};

struct connection {
    struct cache to_server_cache;
    struct cache to_client_cache;
    struct pollfd *server_pollfd;
    struct pollfd *client_pollfd;
};

struct connection_set {
    int max_occupied_pollfd;
    struct connection connections[MAX_CONNECTION_NUM];
};

int running = 1;
struct sockaddr_in relay_addr, my_addr;

int add_line_to_cache(struct cache *cache, const char *line, int len) {
    struct node *buff = (struct node *) malloc(sizeof(struct node));
    if (buff == NULL) return -1;

    memcpy(buff->line, line, len);
    buff->len = len;
    buff->handled_bytes = 0;
    buff->next = NULL;
    buff->prev = cache->last;
    cache->last = buff;
    if (cache->first == NULL)
        cache->first = buff;
    return 0;
}

int add_handled_bytes(struct cache *cache, int bytes_num) {
    struct node *node = cache->first;
    node->handled_bytes += bytes_num;
    if (node->handled_bytes == node->len) {
        cache->first = node->next;
        if (cache->last == node) {
            cache->last = NULL;
        }
        free(node);
    }
    return 0;
}

char *get_line_from_cache(struct cache *cache, int *len) {
    struct node *node = cache->first;
    if (node == NULL) {
        return NULL;
    }
    *len = node->len - node->handled_bytes;
    return node->line + node->handled_bytes;
}

void clear_cache(struct cache *cache) {
    struct node *node = cache->first;
    while (node != NULL) {
        struct node *buff = node;
        node = node->next;
        free(buff);
    }
    cache->first = cache->last = NULL;
}

int set_pollfd(struct connection_set *set, int client_descriptor, int i) {
    int connected_ret, server_descriptor;
    if ((server_descriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Error : Could not create socket \n");
        return -1;
    }
    connected_ret = connect(server_descriptor, (struct sockaddr *) &relay_addr, sizeof(relay_addr));
    if (connected_ret != 0) {
        fprintf(stderr, "Couldn't connect to server: %s\n", strerror(errno));
        return -1;
    }
    set->connections[i].client_pollfd->events =
    set->connections[i].server_pollfd->events = POLLIN | POLLOUT;
    set->connections[i].client_pollfd->fd = client_descriptor;
    set->connections[i].server_pollfd->fd = server_descriptor;
    return 0;
}

int add_connection(struct connection_set *set, int client_descriptor) {
    int i;
    for (i = 0; i < set->max_occupied_pollfd; i++) {
        if (set->connections[i].client_pollfd->events == 0) {
            return set_pollfd(set, client_descriptor, i);
        }
    }
    if (set->max_occupied_pollfd == MAX_CONNECTION_NUM) {
        return -1;
    }
    set->max_occupied_pollfd++;
    return set_pollfd(set, client_descriptor, i);
}

int handle_args(int argc, char *argv[]) {
    int relay_port, listen_port;
    struct in_addr relay_ip;
    if (argc != 4) {
        fprintf(stderr, "Usage: %s listen_port relay_ip relay_port\n", argv[0]);
        return -1;
    }
    listen_port = atoi(argv[1]);
    relay_port = atoi(argv[3]);
    if (listen_port <= 0 || relay_port <= 0) {
        fprintf(stderr, "Usage: %s listen_port relay_ip relay_port\n", argv[0]);
        return -1;
    }
    if (inet_aton(argv[2], &relay_ip) == 0) {
        fprintf(stderr, "Error: inet_aton() failed. Bad relay ip address\n");
        return -1;
    }
    memset(&relay_addr, 0, sizeof(relay_addr));
    relay_addr.sin_family = AF_INET;
    memcpy(&relay_addr.sin_addr.s_addr, &relay_ip, sizeof(struct in_addr));
    relay_addr.sin_port = htons(relay_port);

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr.sin_port = htons(listen_port);
    return 0;
}

int init_listening_socket(int *listen_socket, struct pollfd *accept_fd) {
    *listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (*listen_socket == -1) {
        fprintf(stderr, "Error: socket() failed with %s\n", strerror(errno));
        return -1;
    }
    if (bind(*listen_socket, (struct sockaddr *) &my_addr, sizeof(my_addr))) {
        fprintf(stderr, "Error: bind() failed with %s\n", strerror(errno));
        return -1;
    }
    if (listen(*listen_socket, BACKLOG)) {
        fprintf(stderr, "Error: listen() failed with %s\n", strerror(errno));
        return -1;
    }
    accept_fd->fd = *listen_socket;
    accept_fd->events = POLLIN;
    accept_fd->revents = 0;
    return 0;
}

void sigint_handler(int signum) { running = 0; }

int init_sigint_handler() {
    struct sigaction new_action;
    new_action.sa_handler = sigint_handler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGINT, &new_action, NULL);
    return 0;
}

int init_connection_set(struct connection_set *set, struct pollfd *connection_fds) {
    int i;
    memset(set, 0, sizeof(struct connection_set));
    memset(connection_fds, 0, sizeof(struct pollfd) * MAX_CONNECTION_NUM);
    for (i = 0; i < MAX_CONNECTION_NUM; i++) {
        set->connections[i].server_pollfd = &connection_fds[i * 2];
        set->connections[i].client_pollfd = &connection_fds[i * 2 + 1];
    }
}

int handle_in(struct pollfd *pollfd, struct cache *out_cache) {
    char buff[BUFFER_SIZE];
    int read_bytes;
    if (!(pollfd->revents & POLLIN)) return 0;

    while ((read_bytes = read(pollfd->fd, buff, BUFFER_SIZE)) == -1 &&
           (errno == EINTR));

    if (read_bytes < 0) {
        fprintf(stderr, "Error: read() in handle_connection() failed: %s\n", strerror(errno));
        return -1;
    }
    if (read_bytes == 0) return -1;
    return add_line_to_cache(out_cache, buff, read_bytes);
}

int handle_out(struct pollfd *pollfd, struct cache *in_cache) {
    char *buff;
    int write_bytes, len;
    if (!(pollfd->revents & POLLOUT)) return 0;

    buff = get_line_from_cache(in_cache, &len);
    if (buff == NULL) return 0;
    while ((write_bytes = write(pollfd->fd, buff, len)) == -1 &&
           (errno == EINTR));

    if (write_bytes < 0) {
        fprintf(stderr, "Error: write() failed in handle_out(). %s\n", strerror(errno));
        return -1;
    }
    return add_handled_bytes(in_cache, write_bytes);
}

void close_connection(struct connection_set *set, int i) {
    printf("Connection closed %d\n", i);
    close(set->connections[i].server_pollfd->fd);
    close(set->connections[i].client_pollfd->fd);

    set->connections[i].client_pollfd->fd =
    set->connections[i].server_pollfd->fd = CLOSED_CONNECTION;

    set->connections[i].client_pollfd->events =
    set->connections[i].server_pollfd->events = 0;

    clear_cache(&set->connections[i].to_client_cache);
    clear_cache(&set->connections[i].to_server_cache);
    if (i == set->max_occupied_pollfd - 1) {
        set->max_occupied_pollfd--;
    }
}

int handle_connection(struct connection_set *set, int i) {
    if (set->connections[i].client_pollfd->fd == CLOSED_CONNECTION) return 0;

    if ((set->connections[i].client_pollfd->revents |
         set->connections[i].server_pollfd->revents) &
        (POLLHUP | POLLERR | POLLNVAL)) {
        close_connection(set, i);
        return -1;
    }

    if (handle_in(set->connections[i].client_pollfd, &set->connections[i].to_server_cache) ||
        handle_in(set->connections[i].server_pollfd, &set->connections[i].to_client_cache) ||
        handle_out(set->connections[i].client_pollfd, &set->connections[i].to_client_cache) ||
        handle_out(set->connections[i].server_pollfd, &set->connections[i].to_server_cache)) {
        close_connection(set, i);
        return -1;
    }
}


int handle_connections(struct connection_set *set, int total_changed_pollfds) {
    int i = 0, changed_pollfds_count = 0;
    for (; i < set->max_occupied_pollfd,
                   changed_pollfds_count < total_changed_pollfds; i++) {
        changed_pollfds_count += (set->connections[i].client_pollfd->revents != 0) +
                                 (set->connections[i].server_pollfd->revents != 0);
        handle_connection(set, i);
    }
    return 0;
}

void clear_connection_set(struct connection_set *set) {
    int i;
    for (i = 0; i < set->max_occupied_pollfd; i++) {
        close(set->connections[i].client_pollfd->fd);
        close(set->connections[i].server_pollfd->fd);
        clear_cache(&set->connections[i].to_client_cache);
        clear_cache(&set->connections[i].to_server_cache);
    }
}

void handle_accept(struct pollfd *accept_fd, int *pollret, struct connection_set *set) {
    int new_connection;
    if (accept_fd->revents == 0) return;

    *pollret--;
    if (accept_fd->revents != POLLIN) {
        fprintf(stderr, "Error: listen socket revents is unexpected %d", accept_fd->revents);
        return;
    }
    new_connection = accept(accept_fd->fd, NULL, NULL);
    if (new_connection < 0) {
        fprintf(stderr, "Error: accept() failed with %s", strerror(errno));
        return;
    }
    if (add_connection(set, new_connection) != 0) {
        if (close(new_connection)) {
            fprintf(stderr, "Error: close() failed with %s", strerror(errno));
        }
    }
}

int main(int argc, char *argv[]) {
    struct pollfd fds[FDS_NUM];
    struct pollfd *accept_fd = fds,
            *connection_fds = fds + 1;
    int listen_socket;
    struct connection_set connection_set;

    init_sigint_handler();
    if (handle_args(argc, argv) ||
        init_listening_socket(&listen_socket, accept_fd)) {
        exit(EXIT_FAILURE);
    }
    init_connection_set(&connection_set, connection_fds);

    while (running) {
        int new_connection;
        int nfds = 1 + connection_set.max_occupied_pollfd * 2;
        int pollret = poll(fds, nfds, TIMEOUT);
        if (pollret < 0) {
            perror(strerror(errno));
            continue;
        }
        if (pollret == 0) continue;
        handle_accept(accept_fd, &pollret, &connection_set);
        handle_connections(&connection_set, pollret);
    }

    clear_connection_set(&connection_set);
    if (close(listen_socket) != 0) {
        fprintf(stderr, "Couldn't close server socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
