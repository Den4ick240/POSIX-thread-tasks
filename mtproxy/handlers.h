#ifndef PROXY_HANDLERS_H
#define PROXY_HANDLERS_H

#include "consts.h"

#include "cache.h"
#include "realloc_buffer.h"
#include "picohttpparser.h"

#define HANDLER_FINISHED 1
#define HANDLER_CONTINUE 0
#define HANDLER_EINTR 2
#define HANDLER_ERROR -1

struct server_handler_args {
    int socket;
    struct cache *cache;
    struct cache_reader reader;
    int header_finished_flag;
    struct realloc_buffer header_buffer;
    struct cache_map *cache_map;
};


struct client_handler_args {
    int socket;
    struct cache_reader reader;
    struct cache_map *cache_map;
    struct realloc_buffer request_buffer;

    int (*create_server_handler)(struct server_handler_args *);
};

int server_handle_in(struct server_handler_args *args);

int server_handle_out(struct server_handler_args *args);

int client_handle_in(struct client_handler_args *args);

int client_handle_out(struct client_handler_args *args);

int client_handler_args_init(struct client_handler_args *args, int sockfd,
                             int (*create_server_handler)(struct server_handler_args *), struct cache_map *cache_map);

void destroy_client(struct client_handler_args *client);

void destroy_server(struct server_handler_args *server);


#endif //PROXY_HANDLERS_H
