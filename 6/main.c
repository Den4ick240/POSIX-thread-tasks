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

size_t direntLen;

void *cpFunction(void *arg);

char **allocateCharsets(size_t len1, size_t len2) {
    char **out = (char **) malloc(sizeof(char *) * 2);
    out[0] = (char *) malloc(sizeof(char) * len1);
    out[1] = (char *) malloc(sizeof(char) * len2);
    return out;
}

char **buildNewPath(const char *sourcePath, const char *destinationPath, const char *additionalPath) {
    char **result;
    size_t additionalLen = strlen(additionalPath);
    size_t sourcePathLen = strlen(sourcePath) + additionalLen + 1;
    size_t destinationPathLen = strlen(destinationPath) + additionalLen + 1;

    result = allocateCharsets(sourcePathLen, destinationPathLen);

    strcpy(result[0], sourcePath);
    strcat(result[0], "/");
    strcat(result[0], additionalPath);

    strcpy(result[1], destinationPath);
    strcat(result[1], "/");
    strcat(result[1], additionalPath);
    return result;
}

void freeCharsets(char **sets) {
    free(sets[0]);
    free(sets[1]);
    free(sets);
}

int copyFolder(const char *sourcePath, const char *destinationPath, mode_t mode) {
    DIR *dir;
    struct dirent *entry, *result;
    if (mkdir(destinationPath, mode) == -1 && errno != EEXIST) {
        fprintf(stderr, "Couldn't create directory %s, %s\n", destinationPath, strerror(errno));
        return ERROR_CODE;
    }
    while ((dir = opendir(sourcePath)) == NULL) {
        if (errno != EMFILE) {
            printf("Couldn't open directory %s, %s\n", sourcePath, strerror(errno));
            return ERROR_CODE;
        }
    }
    entry = (struct dirent *) malloc(direntLen);
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
        newPaths = buildNewPath(sourcePath, destinationPath, entry->d_name);
        do {
            pthreadCreateRetVal = pthread_create(&thread, NULL, cpFunction, (void *) newPaths);
        } while (pthreadCreateRetVal != 0 && errno == EAGAIN);
        if (pthreadCreateRetVal != 0) {
            fprintf(stderr, "Couldn't copy %s, %s\n", newPaths[0], strerror(errno));
            free(newPaths);
        }
    }
    free(entry);
    if (closedir(dir) == -1)
        fprintf(stderr, "Couldn't close directory, %s\n", strerror(errno));

    return 0;
}

int copyFile(const char *sourcePath, const char *destinationPath, mode_t mode) {
    static const int OPEN_FILE_FLAGS = O_WRONLY | O_CREAT | O_EXCL;
    int fdin, fdout;
    int bytesRead;
    char buffer[FILE_BUFFER_SIZE];
    int returnValue = 0;

    while ((fdin = open(sourcePath, O_RDONLY)) == -1) {
        if (errno != EMFILE) {
            fprintf(stderr, "Couldn't open file %s, %s\n", sourcePath, strerror(errno));
            return ERROR_CODE;
        }
    }
    while ((fdout = open(destinationPath, OPEN_FILE_FLAGS, mode)) == -1) {
        if (errno != EMFILE) {
            fprintf(stderr, "Couldn't open file %s, %s\n", destinationPath, strerror(errno));
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
    char *sourcePath = ((char **) arg)[0];
    char *destinationPath = ((char **) arg)[1];

    if (stat(sourcePath, &statBuffer) != 0) {
        fprintf(stderr, "%s, %s\n", strerror(errno));
        freeCharsets(arg);
        return (void *) ERROR_CODE;
    }
    if (S_ISDIR(statBuffer.st_mode))
        copyFolder(sourcePath, destinationPath, statBuffer.st_mode);
    else if (S_ISREG(statBuffer.st_mode))
        copyFile(sourcePath, destinationPath, statBuffer.st_mode);
    freeCharsets(arg);
}

char **validateArguments(char **args) {
    struct stat buffer;
    int i;
    if (stat(args[0], &buffer) == -1) {
        fprintf(stderr, "Bad argument: %s - %s\n", args[0], strerror(errno));
        return NULL;
    }
    int len[2] = {strlen(args[0]), strlen(args[1])};
    char **result = allocateCharsets(len[0], len[1]);
    for (i = 0; i < 2; i++) {
        strcpy(result[i], args[i]);
        if (result[i][len[i] - 1] == '/')
            result[i][len[i] - 1] = 0;
    }
    return result;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage %s <copy source> <copy destination>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char **sourceAndDestinationPaths = validateArguments(argv + 1);
    if (sourceAndDestinationPaths == NULL) {
        exit(EXIT_FAILURE);
    }
    ssize_t pathlen = pathconf(sourceAndDestinationPaths[0], _PC_NAME_MAX);
    pathlen = (pathlen == -1 ? 255 : pathlen);
    direntLen = offsetof(struct dirent, d_name) + pathlen + 1;

    cpFunction(sourceAndDestinationPaths);
    pthread_exit(NULL);
}