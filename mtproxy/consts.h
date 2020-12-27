#ifndef PROXY_CONSTS_H
#define PROXY_CONSTS_H

#include "proxytype.h"

#if defined(MULTITHREAD) || defined (THREADPOOL)
#include <pthread.h>
#include <semaphore.h>
#endif
#ifdef THREADPOOL
#include "semaphore.h"
#endif

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <sys/poll.h>

//#define NDEBUG
#include <assert.h>

#define CLIENT_SEND_FLAGS MSG_NOSIGNAL
#define SERVER_SEND_FLAGS MSG_NOSIGNAL

#ifdef MULTITHREAD
#define SERVER_RECV_FLAGS 0
#define CLIENT_RECV_FLAGS 0
#else
#define SERVER_RECV_FLAGS MSG_DONTWAIT
#define CLIENT_RECV_FLAGS MSG_DONTWAIT
#endif

#ifdef SINGLETHREAD
#define THREAD_NUM 1
#endif
#ifdef THREADPOOL
#define THREAD_NUM 8
#endif

#define POLL_TIMEOUT 1000
#define HTTP_MSG_LEN_MAX 256
#define MAX_HOST_NAME_LEN 253
#define DEFAULT_PORT 80
#define DEFAULT_PORT_STRING "80"
#define CLIENT_RECV_BUFFER_LENGTH 4096
#define SERVER_RECV_BUFFER_SIZE 2048
#define NUM_HEADERS 100
#define BACKLOG 510
#define HOST_HEADER_NAME "Host"
#define DEBUG

#define CACHE_MAP_SIZE 2048
#define CACHE_KEY_MAX_SIZE 2048

#endif //PROXY_CONSTS_H
