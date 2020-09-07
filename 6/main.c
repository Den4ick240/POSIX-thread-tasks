#include "pthread.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "dirent.h"
#include "stddef.h"
#include "unistd.h"
#include "stdlib.h"
#include "stdio.h"
#include "errno.h"
#include "string.h"
#include "fcntl.h"

#define STRING_BUFFER_SIZE 1024
#define ERROR_CODE -1
#define FILE_BUFFER_SIZE 8

void *cpFunction(void *arg);

size_t getDirentLen(char *src) {
    static size_t direntLen = 0;
    if (direntLen != 0)
        return direntLen;
    if (src != NULL) {
        ssize_t pathlen = pathconf(src, _PC_NAME_MAX);
        pathlen = (pathlen == -1 ? 255 : pathlen);
        direntLen = offsetof(struct dirent, d_name) + pathlen + 1;
    }
    return 0;
}

char **allocateCharsets(size_t len1, size_t len2) {
    char **out = (char **) malloc(sizeof(char *) * 2);
    out[0] = (char *) malloc(sizeof(char) * len1);
    out[1] = (char *) malloc(sizeof(char) * len2);
    return out;
}

char **buildNewPath(const char *src, const char *dest, const char *add) {
    char **out;
    size_t nameLen = strlen(add);
    size_t lenSrc = strlen(src) + nameLen + 1;
    size_t lenDest = strlen(dest) + nameLen + 1;
    out = allocateCharsets(lenSrc, lenDest);
    strcpy(out[0], src);
    strcat(out[0], "/");
    strcat(out[0], add);
    strcpy(out[1], dest);
    strcat(out[1], "/");
    strcat(out[1], add);
    return out;
}

void freeCharsets(char **sets) {
    free(sets[0]);
    free(sets[1]);
    free(sets);
}

int copyFolder(const char *src, const char *dest, mode_t mode) {
    DIR *dir;
    struct dirent *entry, *result;
    if (mkdir(dest, mode) == -1 && errno != EEXIST) {
        fprintf(stderr, "Couldn't create directory %s, %s\n", dest, strerror(errno));
        return ERROR_CODE;
    }
    while ((dir = opendir(src)) == NULL) {
        if (errno != EMFILE) {
            printf("Couldn't open directory %s, %s\n", src, strerror(errno));
            return ERROR_CODE;
        }
    }
    entry = (struct dirent *) malloc(getDirentLen(NULL));
    if (entry == NULL) {
        perror(strerror(errno));
    }
    while (readdir_r(dir, entry, &result) == 0 && result != NULL) {
        pthread_t thread;
        char **newPaths;
        int pthreadCreateRetVal = 0;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        newPaths = buildNewPath(src, dest, entry->d_name);
        do {
            pthreadCreateRetVal = pthread_create(&thread, NULL, cpFunction, (void *) newPaths);
        } while (pthreadCreateRetVal != 0 && errno == EAGAIN);
        if (pthreadCreateRetVal != 0) {
            fprintf(stderr, "Couldn't copy %s\n", newPaths[0], strerror(errno));
            free(newPaths);
        }
    }
    free(entry);
    if (closedir(dir) == -1)
        fprintf(stderr, "Couldn't close directory, %s\n", strerror(errno));

    return 0;
}

int copyFile(const char *pathSrc, const char *pathDest, mode_t mode) {
    static const int OPEN_FILE_FLAGS = O_WRONLY | O_CREAT | O_EXCL;
    int fdin, fdout;
    int bytesRead;
    char buffer[FILE_BUFFER_SIZE];
    int returnValue = 0;

    while ((fdin = open(pathSrc, O_RDONLY)) == -1) {
        if (errno != EMFILE) {
            fprintf(stderr, "Couldn't open file %s, %s\n", pathSrc, strerror(errno));
            return ERROR_CODE;
        }
    }
    while ((fdout = open(pathDest, OPEN_FILE_FLAGS, mode)) == -1) {
        if (errno != EMFILE) {
            fprintf(stderr, "Couldn't open file %s, %s\n", pathDest, strerror(errno));
            if (close(fdin) != 0)
                perror(strerror(errno));
            return ERROR_CODE;
        }
    }
    while ((bytesRead = read(fdin, buffer, FILE_BUFFER_SIZE)) > 0 || errno == EINTR) {
        char *writePtr = buffer;
        int bytesWritten;
        do {
            bytesWritten = write(fdout, writePtr, bytesRead);
            if (bytesWritten >= 0) {
                bytesRead -= bytesWritten;
                writePtr += bytesWritten;
            } else if (errno != EINTR) {
                perror(strerror(errno));
                returnValue = ERROR_CODE;
                break;
            }
        } while (bytesRead > 0);
    }
    if (bytesRead < 0) {
        perror(strerror(errno));
        returnValue = ERROR_CODE;
    }
    if (close(fdin) != 0) {
        perror(strerror(errno));
        returnValue = ERROR_CODE;
    }
    if (close(fdout) != 0) {
        perror(strerror(errno));
        returnValue = ERROR_CODE;
    }
    return returnValue;
}

void *cpFunction(void *arg) {
    int errorCode;
    struct stat statBuffer;
    char *pathSrc = ((char **) arg)[0];
    char *pathDist = ((char **) arg)[1];

    if (stat(pathSrc, &statBuffer) != 0) {
        fprintf(stderr, "%s, %s\n", strerror(errno));
        freeCharsets(arg);
        return (void *) ERROR_CODE;
    }
    if (S_ISDIR(statBuffer.st_mode))
        copyFolder(pathSrc, pathDist, statBuffer.st_mode);
    else if (S_ISREG(statBuffer.st_mode))
        copyFile(pathSrc, pathDist, statBuffer.st_mode);
    freeCharsets(arg);
}

char **prepareArgs(char **args) {
    struct stat buf;
    int i;
    if (stat(args[0], &buf) == -1) {
        fprintf(stderr, "Bad argument: %s - %s\n", args[0], strerror(errno));
        return NULL;
    }
    int len[2] = {strlen(args[0]), strlen(args[1])};
    char **out = allocateCharsets(len[0], len[1]);
    for (i = 0; i < 2; i++) {
        strcpy(out[i], args[i]);
        if (out[i][len[i] - 1] == '/')
            out[i][len[i] - 1] = 0;
    }
    return out;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage %s <copy source> <copy destination>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char **in = prepareArgs(argv + 1);
    if (in == NULL) {
        exit(EXIT_FAILURE);
    }
    getDirentLen(in[0]);
    cpFunction(in);
    pthread_exit(NULL);
}