// vim: shiftwidth=4 expandtab
#ifndef __ASYNC_QUEUE_H_INCLUDED__
#define __ASYNC_QUEUE_H_INCLUDED__

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include <erl_nif.h>

#ifdef __cplusplus
extern "C" {
#endif

TAILQ_HEAD(queue, async_queue_entry);

struct async_queue_entry {
    TAILQ_ENTRY(async_queue_entry) entries;
    void *data;
};

typedef struct __async_queue {
    struct queue *q;
    ErlNifMutex  *mutex;
    ErlNifCond   *cond;
    int           waiting_threads;
    int           len;
} async_queue_t;

async_queue_t* async_queue_create();
int async_queue_length(async_queue_t *aq);
void* async_queue_pop(async_queue_t *aq);
void async_queue_push(async_queue_t *aq, void *data);
void async_queue_destroy(async_queue_t *aq);

#define ALLOC(size)             enif_alloc(size)
#define MUTEX_LOCK(mutex)       enif_mutex_lock(mutex)
#define MUTEX_UNLOCK(mutex)     enif_mutex_unlock(mutex)
#define MUTEX_DESTROY(mutex)    enif_mutex_destroy(mutex)
#define COND_SIGNAL(condvar)    enif_cond_signal(condvar)
#define COND_DESTROY(condvar)   enif_cond_destroy(condvar)

#ifdef __cplusplus
} // extern "C"
#endif

#endif
