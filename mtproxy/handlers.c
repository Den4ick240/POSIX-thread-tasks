#include "handlers.h"

void try_parsing_response(struct server_handler_args *args, char *buffer, int res) {
    struct phr_header headers[NUM_HEADERS];
    int minor_version, status;
    size_t msg_len, num_headers;
    char msg[HTTP_MSG_LEN_MAX];

    res = realloc_buffer_add_bytes(&args->header_buffer, buffer, res);
    if (res < 0) {
        perror("Realloc buffer for response didn't work");
        args->header_finished_flag = 1;
        realloc_buffer_destroy(&args->header_buffer);
        cache_map_remove(args->cache_map, args->cache);
        return;
    }
    num_headers = NUM_HEADERS;
    res = phr_parse_response((const char *) args->header_buffer.buffer,
                             args->header_buffer.data_len,
                             &minor_version, &status,
                             (const char **) &msg, &msg_len,
                             headers, &num_headers,
                             args->header_buffer.prev_data_len);
    if (res == -2)
        return; //continue receiving request as it was not fully received
    if (res == -1) {
        fprintf(stderr, "Couldn't parse response from server: %s\n", strerror(errno));
        cache_map_remove(args->cache_map, args->cache);
    } else if (status != 200) {
        cache_map_remove(args->cache_map, args->cache);
        puts("Cache removed from map as it its status is not OK");
    }
    printf("---------------------------------------------\n"//Дебажный вывод надо будет убрать
           "received RESPONSE:"
           "%.*s\n"
           "---------------------------------------------\n", (int)args->header_buffer.data_len, args->header_buffer.buffer);
    args->header_finished_flag = 1;
    realloc_buffer_destroy(&args->header_buffer);
}

int server_handle_in(struct server_handler_args *args) {
    char buffer[SERVER_RECV_BUFFER_SIZE];
    int res = recv(args->socket, buffer, SERVER_RECV_BUFFER_SIZE, SERVER_RECV_FLAGS);
    if (res < 0) {
        if (errno == EINTR) return HANDLER_EINTR;
        cache_map_remove(args->cache_map, args->cache);
        fprintf(stderr, "Server recv failed with: %s\n", strerror(errno));
        return HANDLER_ERROR;
    }
    if (!args->header_finished_flag) {
        try_parsing_response(args, buffer, res);
    }
    if (res == 0) {
        return HANDLER_FINISHED;
    }
    if (cache_add_bytes(args->cache, buffer, res) != 0) {
        cache_map_remove(args->cache_map, args->cache);
        return HANDLER_ERROR;
    }
    return HANDLER_CONTINUE;
}

int server_handle_out(struct server_handler_args *args) {
    char *bytes;
    int res;

    res = cache_reader_get_bytes(&args->reader, &bytes, SERVER_OUT_HANDLER_BLOCK_FLAG);
    if (res == ECACHE_FINISHED) {
        cache_reader_release_cache(&args->reader);
        return HANDLER_FINISHED;
    }
    if (res == ECACHE_WOULDBLOCK) return HANDLER_CONTINUE;

    res = send(args->socket, bytes, res, SERVER_SEND_FLAGS);
    if (res < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return HANDLER_CONTINUE;
        } else if (errno == EINTR) {
            return HANDLER_EINTR;
        } else {
            fprintf(stderr, "Server send() failed with: %s\n", strerror(errno));
            return HANDLER_ERROR;
        }
    }
    cache_reader_skip_bytes(&args->reader, res);
    return HANDLER_CONTINUE;
}

int connect_to_server(char *host) {
    int ret, sock;
    char *port = DEFAULT_PORT_STRING;
    struct sockaddr_in server_addr;
    struct addrinfo *ip_struct;
    struct addrinfo hints;
    printf("Connecting to server %s\n", host);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    if ((ret = getaddrinfo(host, port, &hints, &ip_struct)) != 0) {
        fprintf(stderr, "getaddrinfo() failed with %s\n", gai_strerror(ret));
        return -1;
    }
    if ((sock = socket(ip_struct->ai_family, ip_struct->ai_socktype,
                       ip_struct->ai_protocol)) == -1) {
        fprintf(stderr, "Error: socket() failed with %s\n", strerror(errno));
        return -1;
    }
    if (connect(sock, ip_struct->ai_addr, ip_struct->ai_addrlen) != 0) {
        fprintf(stderr, "Error: connect() failed with %s\n", strerror(errno));
        return -1;
    }
    printf("Connected to %s\n", host);
    return sock;
}

int start_server(struct client_handler_args *client, struct cache *cache, char *key, char *host) {
    int res;
    struct server_handler_args *server = (struct server_handler_args *) malloc(sizeof(struct server_handler_args));
    struct cache *server_request_cache;
    if (server == NULL) {
        perror(strerror(errno));
        return -1;
    }
    realloc_buffer_init(&server->header_buffer);
    server->socket = -1;
    server->cache = NULL;
    server->reader.cache_node = NULL;
    server->socket = connect_to_server(host);
    if (server->socket < 0) {
        return -1;
    }
    server_request_cache = cache_create("SERVER REQUEST CACHE");
    if (server_request_cache == NULL) {
        return -1;
    }

    res = cache_add_bytes(server_request_cache, client->request_buffer.buffer, client->request_buffer.data_len);
    if (res < 0) {
        return -1;
    }
    cache_init_reader(server_request_cache, &server->reader);
    cache_finish(server_request_cache);
    cache_release(&server_request_cache);
    server->cache_map = client->cache_map;
    server->cache = cache;
    cache_add_user(cache);
    server->header_finished_flag = 0;
    res = client->create_server_handler(server);
    if (res < 0) {
        return -1;
    }
    return 0;
}


int client_handle_request(struct client_handler_args *client,
                          struct phr_header *headers,
                          size_t num_headers,
                          char *path,
                          size_t path_len,
                          int minor_version,
                          char *method,
                          size_t method_len) {
    char host[MAX_HOST_NAME_LEN];
    char key[CACHE_KEY_MAX_SIZE];
    int cache_created_flag, i;
    struct cache *cache;

    if (minor_version == 9) {
        perror("Unsupported http version\n");
        return HANDLER_ERROR;
    }
    for (i = 0; i != num_headers; ++i) {
        if (strncmp(headers[i].name, HOST_HEADER_NAME, headers[i].name_len) == 0) {
            if (headers[i].value_len < MAX_HOST_NAME_LEN) {
                memcpy(host, headers[i].value, headers[i].value_len);
                host[headers[i].value_len] = '\0';
            } else {
                perror("Host name is too long");
                return HANDLER_ERROR;
            }
            break;
        }
    }
    if (i == num_headers) {
        perror("Host header not found\n");
        return HANDLER_ERROR;
    }
    i = strlen(host);
    if (path_len + i < CACHE_KEY_MAX_SIZE) {
        perror("Path is too long");
    }
    strcpy(key, host);
    strncat(key, path, path_len);
    if (strncmp(method, "GET\0", method_len) != 0) {
        printf("Method %.*s is not supposed to be cached\n", method_len, method);
        cache = cache_create(key);
        cache_created_flag = CACHE_CREATED;
        if (cache == NULL) {
            perror("couldn't create cache for client\n");
            return HANDLER_ERROR;
        }
    } else {
        cache = cache_map_get_or_create(client->cache_map, key, &cache_created_flag);
        if (cache == NULL) {
            perror("couldn't create cache for client\n");
            return HANDLER_ERROR;
        }
    }
    printf("Cache created flag value: %d\n", cache_created_flag);
    if (cache_created_flag == CACHE_CREATED) {
        if (start_server(client, cache, key, host) < 0) {
            cache_release(&cache);
            cache = NULL;
            realloc_buffer_destroy(&client->request_buffer);
            return HANDLER_ERROR;
        }
    }

    cache_init_reader(cache, &client->reader);
    cache_release(&cache);
    realloc_buffer_destroy(&client->request_buffer);
    return HANDLER_FINISHED;
}

int client_handle_in(struct client_handler_args *client) {
    size_t method_len, path_len, num_headers = NUM_HEADERS;
    int pret, minor_version, rret;
    struct phr_header headers[NUM_HEADERS];
    char *method, *path;

    rret = realloc_buffer_recv(client->socket, &client->request_buffer, CLIENT_RECV_BUFFER_LENGTH, CLIENT_RECV_FLAGS);
    if (rret <= 0) {
        if (errno == HANDLER_EINTR) {
            return HANDLER_EINTR;
        }
        fprintf(stderr, "Couldn't read request from client: %s\n", strerror(errno));
        return HANDLER_ERROR;
    }
    pret = phr_parse_request(client->request_buffer.buffer, client->request_buffer.data_len,
                             (const char **) &method, &method_len,
                             (const char **) &path, &path_len,
                             &minor_version, headers, &num_headers, client->request_buffer.prev_data_len);

    if (pret == -2) return HANDLER_CONTINUE; //continue receiving request as it was not fully received
    if (pret == -1) {
        fprintf(stderr, "Couldn't parse request from client: %s\n", strerror(errno));
        return HANDLER_ERROR;
    }
    printf("---------------------------------------------\n"
           "received request:"
           "%.*s\n"
           "---------------------------------------------\n", pret, client->request_buffer.buffer);
    return client_handle_request(client, headers, num_headers, path, path_len, minor_version, method, method_len);
}

int client_handle_out(struct client_handler_args *args) {
    int res;
    char *bytes;
    res = cache_reader_get_bytes(&args->reader, &bytes, SERVER_OUT_HANDLER_BLOCK_FLAG);
    if (res == ECACHE_WOULDBLOCK) return HANDLER_CONTINUE;
    if (res == ECACHE_FINISHED) {
        puts("client finished");
        cache_reader_release_cache(&args->reader);
        return HANDLER_FINISHED;
    }
//    printf("Sending to client  %d\n", res);
    res = send(args->socket, bytes, res, CLIENT_SEND_FLAGS);
//    printf("Sent %d\n", res);
    if (res < 0) {
        if (errno == EINTR) return HANDLER_EINTR;
        fprintf(stderr, "Client send failed with: %s\n", strerror(errno));
        return HANDLER_ERROR;
    }
    cache_reader_skip_bytes(&args->reader, res);
    return HANDLER_CONTINUE;
}

int client_handler_args_init(struct client_handler_args *args,
                             int sockfd,
                             int (*create_server_handler)(struct server_handler_args *),
                             struct cache_map *cache_map) {
    args->create_server_handler = create_server_handler;
    args->socket = sockfd;
    realloc_buffer_init(&args->request_buffer);
    args->cache_map = cache_map;
    args->reader.cache = NULL;
    args->reader.cache_node = NULL;
    args->reader.offset = 0;
    return 0;
}

void destroy_client(struct client_handler_args *client) {
    realloc_buffer_destroy(&client->request_buffer);
    close(client->socket);
    client->socket = -1;
    cache_reader_release_cache(&client->reader);
}

void destroy_server(struct server_handler_args *server) {
    cache_finish(server->cache);
    cache_release(&server->cache);
    cache_reader_release_cache(&server->reader);
    realloc_buffer_destroy(&server->header_buffer);
    close(server->socket);
    server->socket = -1;
    free(server);
}
