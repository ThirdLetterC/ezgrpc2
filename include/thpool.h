/**
 * thpool.h - A C library for thread pools
 * */

#ifndef THPOOL_H
#define THPOOL_H
#include <time.h>
#include "defs.h"
#include "ezgrpc2_list.h"

typedef struct thpool thpool;
struct thpool;

thpool *thpool_new(int workers, int flags);
int thpool_init(thpool *pool, int workers, int flags);
int thpool_add_task(thpool *pool, void (*func)(void *), void *userdata,
                    void (*userdata_cleanup)(void *));
void thpool_cleanup(thpool *pool);
void thpool_free(thpool *pool);
int thpool_is_empty(thpool *pool);
void thpool_stop_and_join(thpool *pool);

#endif
