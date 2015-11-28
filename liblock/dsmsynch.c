/* ########################################################################## */
/* (C) UPMC, 2010-2011                                                        */
/*     Authors:                                                               */
/*       Jean-Pierre Lozi <jean-pierre.lozi@lip6.fr>                          */
/*       Gaël Thomas <gael.thomas@lip6.fr>                                    */
/*       Florian David <florian.david@lip6.fr>                                */
/*       Julia Lawall <julia.lawall@lip6.fr>                                  */
/*       Gilles Muller <gilles.muller@lip6.fr>                                */
/* -------------------------------------------------------------------------- */
/* ########################################################################## */
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/mman.h>
#ifdef __sun__
#include <schedctl.h>
#endif
#include "liblock.h"
#include "liblock-fatal.h"



/* ########################################################################## */
/* Based on "Revisiting the Combining Synchronization Technique" (P. Fatourou */
/* and N. D. Kallimanis, PPoPP'12).                                           */
/* ########################################################################## */

/* Maximum number of iterations by the combiner */
#ifndef MAX_COMBINER_ITERATIONS
#define MAX_COMBINER_ITERATIONS (topology->nb_nodes * 3)
#endif


// SFENCE
#if defined(__i386__) || defined(__x86_64__)
#define SFENCE()  asm volatile("sfence" : : )
#elif defined(__sparc__)
#define SFENCE() asm volatile ("membar #StoreLoad" : : : "memory");
#endif

// CAS
#define CAS __sync_val_compare_and_swap

// SWAP
#define SWAP __sync_lock_test_and_set


/* Node */
typedef struct dsmsynch_node {
    void *(*volatile                        req)(void *);
    void *volatile                          ret;
    volatile char                           wait;
    volatile char                           completed;
    volatile struct dsmsynch_node *volatile next;
    char __pad[pad_to_cache_line(sizeof(void *(*)(void *)) +
                                 sizeof(void *) +
                                 sizeof(char) +
                                 sizeof(char) +
                                 sizeof(struct dsmsynch_node *))];
} dsmsynch_node;

/* The lock itself (only contains the global tail) */
struct liblock_impl {
    volatile dsmsynch_node *volatile         tail;
    unsigned int                             lock_id;
    char __pad[pad_to_cache_line(sizeof(dsmsynch_node *))];
};


/* Local node */
// FIXME? Only two nodes per threads might be enough.
static __thread volatile dsmsynch_node *my_nodes[2] = {NULL, NULL};
static __thread int *toggles = 0;
#ifdef __sun__
/* Scheduler hint */
static __thread schedctl_t *schedctl;
#endif

static volatile unsigned int cur_lock_number = 0;



static void build_local_data()
{
    int i, j;

    toggles = liblock_allocate(sizeof(int) * MAX_LOCKS);
    bzero(toggles, sizeof(int) * MAX_LOCKS);

    for (i = 0; i < 2; i++)
    {
        my_nodes[i] = liblock_allocate(sizeof(dsmsynch_node) * MAX_LOCKS);

        for (j = 0; j < MAX_LOCKS; j++)
        {
            my_nodes[i][j].req = NULL;
            my_nodes[i][j].ret = NULL;
            my_nodes[i][j].wait = 0;
            my_nodes[i][j].completed = 0;
            my_nodes[i][j].next = NULL;
        }
    }
}

static struct liblock_impl *do_liblock_init_lock(dsmsynch)
                                (liblock_lock_t *lock,
                                 struct hw_thread *core,
                                 pthread_mutexattr_t *attr)
{
    struct liblock_impl *impl = liblock_allocate(sizeof(struct liblock_impl));

    impl->tail = NULL;
    impl->lock_id = __sync_fetch_and_add(&cur_lock_number, 1);


#ifdef __sparc__
    __sync_synchronize();
#else
    asm volatile ("mfence":::"memory");
#endif
    return impl;
}

static int do_liblock_destroy_lock(dsmsynch)(liblock_lock_t *lock)
{
    return 0;
}

static void *do_liblock_execute_operation(dsmsynch)(liblock_lock_t *lock,
                                                   void *(*pending)(void *),
                                                   void *val)
{
    volatile struct liblock_impl *impl = lock->impl;
    volatile dsmsynch_node *tmp_node, *my_node, *my_pred_node;
    int counter = 0;


    if(my_nodes[0] == NULL)
        build_local_data();

    toggles[impl->lock_id] = 1 - toggles[impl->lock_id];
    my_node = &my_nodes[toggles[impl->lock_id]][impl->lock_id];
    my_node->wait = 1;
    my_node->completed = 0;
    my_node->next = NULL;

    my_node->req = pending;
    my_node->ret = val;

#ifdef __sun__
    schedctl_start(schedctl);
#endif

    my_pred_node = SWAP(&impl->tail, my_node);

    if (my_pred_node)
    {
        my_pred_node->next = my_node;

        SFENCE();

#ifdef __sun__
        schedctl_stop(schedctl);
#endif

        while (my_node->wait)
#ifdef WITH_YIELD
        YIELD();
#else
        PAUSE();
#endif

        if (my_node->completed)
            return my_node->ret;
    }

#ifdef __sun__
    schedctl_start(schedctl);
#endif

    tmp_node = my_node;

    for (;;)
    {
        // We use these prefetch parameters since they are the ones used in
        // the official implementation of CC-Synch/DSM-Synch.
        __builtin_prefetch((struct dsmsynch_node *)tmp_node->next, 0, 3);

        counter++;

        tmp_node->ret = (*tmp_node->req)(tmp_node->ret);
        tmp_node->completed = 1;
        tmp_node->wait = 0;

        if (!tmp_node->next || !tmp_node->next->next ||
            counter >= MAX_COMBINER_ITERATIONS)
            break;

        tmp_node = tmp_node->next;
    }

    if (!tmp_node->next)
    {
        if (CAS(&impl->tail, tmp_node, NULL) == tmp_node)
            return my_node->ret;

        while (!(*(volatile struct dsmsynch_node *volatile *)&tmp_node->next))
            YIELD();
    }

    tmp_node->next->wait = 0;

    SFENCE();

#ifdef __sun__
    schedctl_stop(schedctl);
#endif

    return my_node->ret;
}

static void do_liblock_init_library(dsmsynch)()
{}

static void do_liblock_kill_library(dsmsynch)()
{}

static void do_liblock_run(dsmsynch)(void (*callback)())
{
    if(__sync_bool_compare_and_swap(&liblock_start_server_threads_by_hand,
                                    1, 0) != 1)
        fatal("servers are not managed manually");
    if(callback)
        callback();
}

static int do_liblock_cond_init(dsmsynch)(liblock_cond_t *cond)
{
    fatal("not implemented");
}

static int do_liblock_cond_timedwait(dsmsynch)(liblock_cond_t *cond,
                                              liblock_lock_t *lock,
                                              const struct timespec* ts)
{
    fatal("not implemented");
}

static int do_liblock_cond_wait(dsmsynch)(liblock_cond_t *cond,
                                         liblock_lock_t *lock)
{
    fatal("not implemented");
}

static int do_liblock_cond_signal(dsmsynch)(liblock_cond_t *cond)
{
    fatal("not implemented");
}

static int do_liblock_cond_broadcast(dsmsynch)(liblock_cond_t *cond)
{
    fatal("not implemented");
}

static int do_liblock_cond_destroy(dsmsynch)(liblock_cond_t *cond)
{
    fatal("not implemented");
}

static void do_liblock_on_thread_start(dsmsynch)(struct thread_descriptor *desc)
{
#ifdef __sun__
    schedctl = schedctl_init();
#endif
}

static void do_liblock_on_thread_exit(dsmsynch)(struct thread_descriptor *desc)
{
    // TODO: Free local data
}

static void do_liblock_unlock_in_cs(dsmsynch)(liblock_lock_t *lock)
{
    fatal("not implemented");
}

static void do_liblock_relock_in_cs(dsmsynch)(liblock_lock_t *lock)
{
    fatal("not implemented");
}

static void do_liblock_declare_server(dsmsynch)(struct hw_thread *core)
{}

static void do_liblock_cleanup(dsmsynch)(void)
{
    // All mmap'd/malloc'd variables seem to be released cleanly for this lock.
}

liblock_declare(dsmsynch);

