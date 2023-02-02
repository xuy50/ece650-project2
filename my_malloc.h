#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

struct metadata
{
    size_t size;
    int isFree;
    struct metadata *prev;
    struct metadata *next;
};
typedef struct metadata md_t;

//Thread Safe malloc/free: locking version
void *ts_malloc_lock(size_t size);
void ts_free_lock(void *ptr);

//Thread Safe malloc/free: non-locking version
void *ts_malloc_nolock(size_t size);
void ts_free_nolock(void *ptr);

unsigned long get_data_segment_size();            // in bytes
unsigned long get_data_segment_free_space_size(); // in byte