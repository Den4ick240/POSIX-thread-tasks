#ifndef PROXY_REALLOC_BUFFER_H
#define PROXY_REALLOC_BUFFER_H

#include "consts.h"

struct realloc_buffer {
    char *buffer;
    size_t data_len;
    size_t prev_data_len;
    size_t buffer_size;
};

void realloc_buffer_init(struct realloc_buffer *realloc_buffer);

int realloc_buffer_add_bytes(struct realloc_buffer *realloc_buffer, char *bytes, int len);

int realloc_buffer_recv(int sockfd, struct realloc_buffer *realloc_buffer, int max_len, int recv_flags);

int realloc_buffer_send(int sockfd, struct realloc_buffer *realloc_buffer, int offset, int max_len, int recv_flags);

int realloc_buffer_remove_bytes_at_start(struct realloc_buffer *realloc_buffer, int n);

void realloc_buffer_destroy(struct realloc_buffer *realloc_buffer);

#endif //PROXY_REALLOC_BUFFER_H
