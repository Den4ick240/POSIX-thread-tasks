#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "signal.h"
#define EXIT_FLAG_CHECK_INTERVAL 100

pthread_mutex_t exitFlagMutex;
int exitFlag = 0;
unsigned long long int maxIter = 0;

void sigintHandler(int unused) {
    static pthread_t thread;
    pthread_mutex_lock(&exitFlagMutex);
    exitFlag = 1;
    pthread_mutex_unlock(&exitFlagMutex);
}

double getRowMember(unsigned int i) {
    return (i % 2 == 0 ? 1.0 : -1.0) / (1.0 + i * 2.0);
}

void *threadComputer(void *arg) {
    int start = ((int *) arg)[0];
    int step = ((int *) arg)[1];
    double *out = (double *) malloc(sizeof(double));
    unsigned long long int i, j;
    free(arg);
    if (out == NULL) {
        perror(strerror(errno));
        pthread_exit(NULL);
    }
    *out = 0;
    for (i = start, j = 0; 1; i += step, j++) {
        *out += getRowMember(i);
        if (j % EXIT_FLAG_CHECK_INTERVAL == 0) {
            pthread_mutex_lock(&exitFlagMutex);
            if (maxIter == j) {
                if (exitFlag != 0) {
                    pthread_mutex_unlock(&exitFlagMutex);
                    break;
                } else {
                    maxIter += EXIT_FLAG_CHECK_INTERVAL;
                }
            }
            pthread_mutex_unlock(&exitFlagMutex);
            sched_yield();
        }
    }
    pthread_exit(out);
}

double run(int threadNum) {
    int i;
    double result = 0;
    pthread_t *threads = (pthread_t *) malloc(sizeof(pthread_t) * threadNum);
    if (threads == NULL) {
        perror(strerror(errno));
        return -1.0;
    }
    for (i = 0; i < threadNum; i++) {
        int *arg = (int *) malloc(sizeof(int) * 2);
        if (arg == NULL) {
            perror(strerror(errno));
            return -1.0;
        }
        arg[0] = i;
        arg[1] = threadNum;
        if (pthread_create(threads + i, NULL, threadComputer, arg) != 0) {
            fprintf(stderr, "Couldn't create a thread number %d, %s\n", i, strerror(errno));
            break;
        }
    }
    while (i--) {
        double *retVal;
        if (pthread_join(threads[i], (void **) &retVal) != 0) {
            fprintf(stderr, "Couldn't join thread, %s\n", strerror(errno));
            continue;
        }
        if (retVal == NULL) {
            fprintf(stderr, "Error occurred during computations\n");
            continue;
        }
        result += *retVal;
        free(retVal);
    }
    return result * 4;
}

int main(int argc, char *argv[]) {
    int threadNum;
    char *endptr = NULL;
    double result;
    if (argc < 2) {
        printf("Usage: %s numberOfThreads\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if ((threadNum = strtol(argv[1], &endptr, 10)) <= 0 || *endptr != 0) {
        puts("numberOfThreads must be a positive number");
    }
    signal(SIGINT, sigintHandler);
    pthread_mutex_init(&exitFlagMutex, NULL);
    result = run(threadNum);
    if (result == -1.0) {
        puts("An error occurred");
    } else {
        printf("pi equals %lf\n", result);
    }
    pthread_mutex_destroy(&exitFlagMutex);
    exit(EXIT_SUCCESS);
}


