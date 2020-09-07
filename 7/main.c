#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"

#define NUM_OF_STEPS 200000000


double getRowMember(unsigned int i) {
    return (i % 2 == 0 ? 1.0 : -1.0) / (1.0 + i * 2.0);
}

void *threadComputer(void *arg) {
    int start = ((int *)arg)[0];
    int step = ((int *)arg)[1];
    double *out = (double *)malloc(sizeof(double));
    unsigned int i;
    free(arg);
    if (out == NULL) {
        perror(strerror(errno));
        pthread_exit(NULL);
    }
    *out = 0;
    for (i = start; i < NUM_OF_STEPS; i += step) {
        *out += getRowMember(i);
    }
    pthread_exit(out);
}

double run(int threadNum) {
    int i;
    double result = 0;
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * threadNum);
    if (threads == NULL) {
        perror(strerror(errno));
        return -1.0;
    }
    for (i = 0; i < threadNum; i++) {
        int *arg = (int *)malloc(sizeof(int) * 2);
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
        pthread_join(threads[i], (void **)&retVal);
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
    result = run(threadNum);
    if (result == -1.0) {
        puts("An error occurred");
    }
    else {
        printf("pi equals %lf\n", result);
    }
    exit(EXIT_SUCCESS);
}


