#include "cache.h"

struct cache_node {
    struct cache_node *next;
    int data_len;
    char bytes[];
};

struct cache {
#if defined(MULTITHREAD) || defined(THREADPOOL)
    struct cond_rwlock cond_rwlock;             //this rwlock used to synchronize reading from cache and writing to cache
    pthread_mutex_t mutex;                      //this mutex used to synchronize access to the users_cnt variable
#endif
    struct timeval last_used_time;              //this time is updated when someone stops using cache
    int finished;                               //if this flag is not zero, than no one supposed to write data to this cache anymore
    int users_cnt;                              //number of threads, using this cache. When every thread calls cache_release(),
                                                //this variable becomes 0 and than the cache is deleted
    struct cache_node *first, *last;            //first and last elements of the queue
    char key[CACHE_KEY_MAX_SIZE];               //key associated with that cache, usually it is host + path parsed from http request
};

int update_time_func(struct timeval *time) {
    return gettimeofday(time, NULL);
}

int cache_map_init(struct cache_map *cache_map) {
#if defined(MULTITHREAD) || defined(THREADPOOL)
    int res = pthread_mutex_init(&cache_map->mutex, NULL);
    if (res < 0) return res;
#endif
    arrayset_init(&cache_map->arrayset);
    return 0;
}

int remove_oldest_cache(struct cache_map *cache_map) {
    struct cache *latest_cache = NULL, *cache;
    int i;
    for (i = 0; i < cache_map->arrayset.data_size; i++) {
        cache = (struct cache *) cache_map->arrayset.arr[i];
        if (cache->users_cnt > 1) continue;
        if (latest_cache == NULL) {
            latest_cache = cache;
            continue;
        }
        if (timercmp(&cache->last_used_time, &latest_cache->last_used_time, <)) latest_cache = cache;
    }
    if (latest_cache == NULL) {
        return -1;
    }
    arrayset_remove(&cache_map->arrayset, cache);
    cache_release(&cache);
    puts("Oldest cache removed");
    return 0;
}

struct cache *cache_map_get_or_create(struct cache_map *cache_map, char *key, int *cache_flag) {
    int i;
    struct cache *cache = NULL;
#if defined(MULTITHREAD) || defined(THREADPOOL)
    pthread_mutex_lock(&cache_map->mutex);
#endif
    puts("Looking for cache in cache map");
    puts(key);
    for (i = 0; i < cache_map->arrayset.data_size; i++) {
        cache = (struct cache *) cache_map->arrayset.arr[i];
        puts(cache->key);
        if (strcmp(key, cache->key) == 0) {
            *cache_flag = CACHE_FOUND;
            break;
        }
        cache = NULL;
    }
    if (cache == NULL) {
        puts("No cache found");
        if (cache_map->arrayset.data_size == CACHE_MAP_SIZE && remove_oldest_cache(cache_map) != 0) {
#if defined(MULTITHREAD) || defined(THREADPOOL)
            pthread_mutex_unlock(&cache_map->mutex);
#endif
            return NULL;
        }
        cache = cache_create(key);
        *cache_flag = CACHE_CREATED;
        if (cache == NULL) {
            perror("Couldn't create cache");
        } else {
            arrayset_add(&cache_map->arrayset, cache);
        }
    }
#if defined(MULTITHREAD) || defined(THREADPOOL)
    pthread_mutex_unlock(&cache_map->mutex);
#endif
    if (cache != NULL) cache_add_user(cache);
    return cache;
}

int cache_map_remove(struct cache_map *cache_map, struct cache *cache) {
    int res;
    puts("Removing element from cache map");
#if defined(MULTITHREAD) || defined(THREADPOOL)
    pthread_mutex_lock(&cache_map->mutex);
#endif
    res = arrayset_remove(&cache_map->arrayset, cache);
    if (res == 0)
        cache_release(&cache);
#if defined(MULTITHREAD) || defined(THREADPOOL)
    pthread_mutex_unlock(&cache_map->mutex);
#endif
    return 0;
}

void free_elem(void *elem) {
    cache_release((struct cache **) &elem);
}

int cache_map_destroy(struct cache_map *cache_map) {
#if defined(MULTITHREAD) || defined(THREADPOOL)
    int res = pthread_mutex_destroy(&cache_map->mutex);
    if (res < 0) return res;
#endif
    arrayset_free(&cache_map->arrayset, free_elem);
    return 0;
}

struct cache *cache_create(char *key) {
    struct cache *cache = (struct cache *) malloc(sizeof(struct cache));
    puts("Creating cache");
    if (cache == NULL) {
        perror("couldn't allocate cacche structure");
        return NULL;
    }
    strncpy(cache->key, key, CACHE_KEY_MAX_SIZE);
    printf("cache: %.*s created\n", CACHE_KEY_MAX_SIZE, cache->key);
#if defined(MULTITHREAD) || defined(THREADPOOL)
    int res = cond_rwlock_init(&cache->cond_rwlock);
    if (res < 0) {
        free(cache);
        return NULL;
    }
    res = pthread_mutex_init(&cache->mutex, NULL);
    if (res < 0) {
        cond_rwlock_destroy(&cache->cond_rwlock);
        free(cache);
        return NULL;
    }
#endif
    update_time_func(&cache->last_used_time);
    cache->users_cnt = 1;
    cache->first = NULL;
    cache->last = NULL;
    cache->finished = 0;
    return cache;
}

void cache_release(struct cache **_cache) {
    struct cache *cache = *_cache;
    *_cache = NULL;
    if (cache == NULL) return;
#if defined(MULTITHREAD) || defined(THREADPOOL)
    pthread_mutex_lock(&cache->mutex);
#endif
    cache->users_cnt--;
    printf("%s:\n cache users: %d\n", cache->key, cache->users_cnt);
    update_time_func(&cache->last_used_time);
#if defined(MULTITHREAD) || defined(THREADPOOL)
    pthread_mutex_unlock(&cache->mutex);
#endif
    if (cache->users_cnt == 0) {
        struct cache_node *node = cache->first;
        printf("Deleting cache %s, as all users released it\n", cache->key);
        while (node != NULL) {
            struct cache_node *buff = node;
            node = node->next;
            free(buff);
        }
        free(cache);
#if defined(MULTITHREAD) || defined(THREADPOOL)
        cond_rwlock_destroy(&cache->cond_rwlock);
        pthread_mutex_destroy(&cache->mutex);
#endif
    }
}

void cache_add_user(struct cache *cache) {
#if defined(MULTITHREAD) || defined(THREADPOOL)
    pthread_mutex_lock(&cache->mutex);
#endif
    cache->users_cnt++;
    printf("%s:\n cache users: %d\n", cache->key, cache->users_cnt);
#if defined(MULTITHREAD) || defined(THREADPOOL)
    pthread_mutex_unlock(&cache->mutex);
#endif
}

void cache_finish(struct cache *cache) {
    cache->finished = 1;
    cond_rwlock_drop(&cache->cond_rwlock);
}

int cache_add_bytes(struct cache *cache, char *bytes, int len) {
    struct cache_node *node = (struct cache_node *) malloc(sizeof(struct cache_node) + sizeof(char) * len);
    if (node == NULL) {
        fprintf(stderr, "Couldn't add bytes to cache: %s\n", strerror(errno));
        return -1;
    }
    memcpy(node->bytes, bytes, sizeof(char) * len);
    node->data_len = len;
    node->next = NULL;
#if defined(MULTITHREAD) || defined(THREADPOOL)
    cond_rwlock_wrlock(&cache->cond_rwlock);
#endif
    if (cache->first == NULL) {
        cache->last = node;
        cache->first = node;
    } else {
        cache->last->next = node;
        cache->last = node;
    }
#if defined(MULTITHREAD) || defined(THREADPOOL)
    cond_rwlock_wrunlock(&cache->cond_rwlock);
#endif
    return 0;
}

void cache_init_reader(struct cache *cache, struct cache_reader *reader) {
    reader->cache = cache;
    reader->offset = 0;
    if (cache == NULL) {
        puts("Initializing reader on NULL cache");
        return;
    }
#if defined(MULTITHREAD) || defined(THREADPOOL)
    cond_rwlock_rdlock(&cache->cond_rwlock);
#endif
    reader->cache_node = cache->first;
#if defined(MULTITHREAD) || defined(THREADPOOL)
    cond_rwlock_rdunlock(&cache->cond_rwlock);
#endif
    cache_add_user(cache);
}

int cache_reader_has_data_available(struct cache_reader *reader) {
    return (reader->cache_node == NULL && reader->cache->first != NULL) ||
           (reader->cache_node != NULL &&
            (reader->cache_node->next != NULL ||
             reader->offset < reader->cache_node->data_len));
}

int predicate(void *arg) {
    return cache_reader_has_data_available((struct cache_reader *) arg);
}

int cache_reader_get_bytes(struct cache_reader *reader, char **buffer, int block_flag) {
    struct cache *cache = reader->cache;
    int res;

    if (reader->cache_node != NULL && reader->offset < reader->cache_node->data_len) {
        *buffer = reader->cache_node->bytes + reader->offset;
        return reader->cache_node->data_len - reader->offset;
    }

#if defined(MULTITHREAD) || defined(THREADPOOL)
    if (block_flag) {
        cond_rwlock_wait_and_rdlock(&cache->cond_rwlock, predicate, reader);
    } else {
        cond_rwlock_rdlock(&cache->cond_rwlock);
    }
#endif
    if (!cache_reader_has_data_available(reader)) {
        res = (cache->finished ? ECACHE_FINISHED : ECACHE_WOULDBLOCK);
    } else {
        if (reader->cache_node == NULL) {
            reader->cache_node = cache->first;
            reader->offset = 0;
        }
        if (reader->cache_node->data_len == reader->offset) {
            reader->cache_node = reader->cache_node->next;
            reader->offset = 0;
        }
        *buffer = reader->cache_node->bytes + reader->offset;
        res = reader->cache_node->data_len - reader->offset;
    }
#if defined(MULTITHREAD) || defined(THREADPOOL)
    cond_rwlock_rdunlock(&cache->cond_rwlock);
#endif
    return res;
}

int cache_reader_skip_bytes(struct cache_reader *reader, int bytes_num) {
    reader->offset += bytes_num;
    assert(reader->offset <= reader->cache_node->data_len);
    if (reader->offset != reader->cache_node->data_len) return 0;
}

void cache_reader_release_cache(struct cache_reader *reader) {
    cache_release(&reader->cache);
    reader->cache = NULL;
}


