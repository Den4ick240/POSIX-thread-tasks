#include "realloc_buffer.h"

int increase_buffer_size_to_fit_n_more_bytes(struct realloc_buffer *realloc_buffer, int n) {
    int new_buffer_size = realloc_buffer->data_len + n;
    if (n < 0) {
        errno = EINVAL;
        return -1;
    }
    if (new_buffer_size > realloc_buffer->buffer_size) {
        realloc_buffer->buffer = (char *) realloc(realloc_buffer->buffer, new_buffer_size);
        if (realloc_buffer->buffer == NULL) {
            realloc_buffer->buffer_size = -1;
            return -1;
        }
        realloc_buffer->buffer_size = new_buffer_size;
    }
    return 0;
}

void realloc_buffer_init(struct realloc_buffer *realloc_buffer) {
    realloc_buffer->buffer = NULL;
    realloc_buffer->data_len = 0;
    realloc_buffer->prev_data_len = 0;
    realloc_buffer->buffer_size = 0;
}

int realloc_buffer_add_bytes(struct realloc_buffer *realloc_buffer, char *bytes, int len) {
    if (increase_buffer_size_to_fit_n_more_bytes(realloc_buffer, len) != 0) {
        return -1;
    }
    memcpy(realloc_buffer->buffer + realloc_buffer->data_len, bytes, len);
    realloc_buffer->prev_data_len = realloc_buffer->data_len;
    realloc_buffer->data_len += len;
    return 0;
}


int realloc_buffer_recv(int sockfd, struct realloc_buffer *realloc_buffer, int max_len, int recv_flags) {
    int recv_res;
    char *recv_buffer;
    if (increase_buffer_size_to_fit_n_more_bytes(realloc_buffer, max_len) != 0) {
        perror("couldn't increase realloc buffer");
        return -1;
    }
    recv_buffer = realloc_buffer->buffer + realloc_buffer->data_len;
    recv_res = recv(sockfd, recv_buffer, max_len, recv_flags);
    if (recv_res > 0) {
        realloc_buffer->prev_data_len = realloc_buffer->data_len;
        realloc_buffer->data_len += recv_res;
    }
    return recv_res;
}

int realloc_buffer_send(int sockfd, struct realloc_buffer *realloc_buffer, int offset, int max_len, int recv_flags) {
    int res;
    char *send_buffer = realloc_buffer->buffer + offset;
    int send_len = realloc_buffer->data_len - offset;
    if (send_len < 0) {
        errno = EINVAL;
        return -1;
    }
    send_len = (send_len > max_len ? max_len : send_len);
    do {
        res = send(sockfd, send_buffer, send_len, recv_flags);
    } while (res == -1 && errno == EINTR);
    return res;
}

int realloc_buffer_remove_bytes_at_start(struct realloc_buffer *realloc_buffer, int n) {
    int new_size = realloc_buffer->data_len - n;
    if (new_size <= 0) {
        free(realloc_buffer->buffer);
        realloc_buffer->data_len = realloc_buffer->buffer_size = 0;
        realloc_buffer->buffer = NULL;
        return 0;
    }
    char *new_buffer = (char *) malloc(sizeof(char) * new_size);
    if (new_buffer == NULL) {
        return -1;
    }
    memcpy(new_buffer, realloc_buffer->buffer + n, new_size);
    free(realloc_buffer->buffer);
    realloc_buffer->buffer = new_buffer;
    realloc_buffer->buffer_size = realloc_buffer->data_len = new_size;
    return 0;
}

void realloc_buffer_destroy(struct realloc_buffer *realloc_buffer) {
    free(realloc_buffer->buffer);
    realloc_buffer_init(realloc_buffer);
}