#ifndef PROXY_CACHE_H
#define PROXY_CACHE_H

#include "consts.h"
#include "arrayset.h"

#if defined(MULTITHREAD) || defined(THREADPOOL)
#include "condrwlock.h"
#endif

#define ECACHE_FINISHED -1
#define ECACHE_WOULDBLOCK -2

#define CACHE_CREATED 1
#define CACHE_FOUND 2


struct cache;
struct cache_reader {
    struct cache *cache;
    struct cache_node *cache_node;
    int offset;
};

#if defined(MULTITHREAD) || defined(THREADPOOL)
#define CACHE_MAP_INITIALIZER { ARRAY_SET_INITIALIZER, PTHREAD_MUTEX_INITIALIZER }
#else
#define CACHE_MAP_INITIALIZER { ARRAY_SET_INITIALIZER }
#endif

struct cache_map {
    struct arrayset arrayset;
#if defined(MULTITHREAD) || defined(THREADPOOL)
    pthread_mutex_t mutex;
#endif
};

int cache_map_init(struct cache_map *cache_map);

struct cache *cache_map_get_or_create(struct cache_map *cache_map, char *key, int *cache_flag);

int cache_map_remove(struct cache_map *cache_map, struct cache *cache);

int cache_map_destroy(struct cache_map *cache_map);

struct cache *cache_create(char *key);

//has inner counter of users, when last user releases the cache, this function destroys it.
void cache_release(struct cache **_cache);

void cache_add_user(struct cache *cache);

void cache_finish(struct cache *cache);

//int cache_is_finished(struct cache *cache);

int cache_add_bytes(struct cache *cache, char *bytes, int len);

void cache_init_reader(struct cache *cache, struct cache_reader *reader);

//if block_flag is not 0, cache is not finished but doesn't have new data at the moment, this function will block until there is new data available
int cache_reader_get_bytes(struct cache_reader *reader, char **buffer, int block_flag);

int cache_reader_skip_bytes(struct cache_reader *reader, int bytes_num);

void cache_reader_release_cache(struct cache_reader *reader);

#endif //PROXY_CACHE_H
