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
#define SFENCE()  asm volatile("sfence"::)
#elif defined(__sparc__)
#define SFENCE() asm volatile ("membar #StoreLoad" : : : "memory");
#endif

// SWAP
#define SWAP __sync_lock_test_and_set


/* Node */
typedef struct ccsynch_node {
    void *(*volatile                       req)(void *);
    void *volatile                         ret;
    volatile char                          wait;
    volatile char                          completed;
    volatile struct ccsynch_node *volatile next;
    char __pad[pad_to_cache_line(sizeof(void *(*)(void *)) +
                                 sizeof(void *) +
                                 sizeof(char) +
                                 sizeof(char) +
                                 sizeof(struct ccsynch_node *))];
} ccsynch_node;

/* The lock itself (only contains the global tail) */
struct liblock_impl {
    volatile ccsynch_node *volatile        tail;
    unsigned int                           lock_id;
    char __pad[pad_to_cache_line(sizeof(ccsynch_node *))];
};


/* Local node */
// FIXME? Only two nodes per threads might be enough.
static __thread volatile ccsynch_node **my_nodes = NULL;
#ifdef __sun__
/* Scheduler hint */
static __thread schedctl_t *schedctl;
#endif

static volatile unsigned int cur_lock_number = 0;



static void build_local_data()
{
    int i;

    my_nodes = liblock_allocate(sizeof(ccsynch_node *) * MAX_LOCKS);

    for (i = 0; i < MAX_LOCKS; i++)
    {
        my_nodes[i] = liblock_allocate(sizeof(ccsynch_node) * MAX_LOCKS);

        my_nodes[i]->req = NULL;
        my_nodes[i]->ret = NULL;
        my_nodes[i]->wait = 0;
        my_nodes[i]->completed = 0;
        my_nodes[i]->next = NULL;
    }
}

static struct liblock_impl *do_liblock_init_lock(ccsynch)
                                (liblock_lock_t *lock,
                                 struct hw_thread *core,
                                 pthread_mutexattr_t *attr)
{
    struct liblock_impl *impl = liblock_allocate(sizeof(struct liblock_impl));

    impl->tail = anon_mmap(r_align(sizeof(ccsynch_node), PAGE_SIZE));

    impl->tail->req = NULL;
    impl->tail->ret = NULL;
    impl->tail->wait = 0;
    impl->tail->completed = 0;
    impl->tail->next = NULL;

    impl->lock_id = __sync_fetch_and_add(&cur_lock_number, 1);

    SFENCE();

    return impl;
}

static int do_liblock_destroy_lock(ccsynch)(liblock_lock_t *lock)
{
    /* No need to unmap impl ? */
    munmap((void *)lock->impl->tail, r_align(sizeof(ccsynch_node), PAGE_SIZE));

    return 0;
}

static void *do_liblock_execute_operation(ccsynch)(liblock_lock_t *lock,
                                                   void *(*pending)(void*),
                                                   void *val)
{
    volatile struct liblock_impl *impl = lock->impl;
    volatile ccsynch_node *next_node, *cur_node, *tmp_node, *tmp_node_next;
    int counter = 0;


    if(my_nodes == NULL)
        build_local_data();

    next_node = my_nodes[impl->lock_id];
    next_node->next = NULL;
    next_node->wait = 1;
    next_node->completed = 0;

#ifdef __sun__
    schedctl_start(schedctl);
#endif

    cur_node = SWAP(&impl->tail, next_node);
    cur_node->req = pending;
    cur_node->ret = val;
    cur_node->next = next_node;

#ifdef __sun__
    schedctl_stop(schedctl);
#endif


    my_nodes[impl->lock_id] = cur_node;

    while (cur_node->wait)
#ifdef WITH_YIELD
        YIELD();
#else
        PAUSE();
#endif

    if (cur_node->completed)
        return cur_node->ret;

#ifdef __sun__
    schedctl_start(schedctl);
#endif

    tmp_node = cur_node;

    while (tmp_node->next && counter < MAX_COMBINER_ITERATIONS)
    {
        // We use these prefetch parameters since they are the ones used in                                                                                 // the official implementation of CC-Synch/DSM-Synch
        __builtin_prefetch((struct ccsynch_node *)tmp_node->next, 0, 3);

        counter++;

        tmp_node_next = tmp_node->next;

        tmp_node->ret = (*tmp_node->req)(tmp_node->ret);
        tmp_node->completed = 1;
        tmp_node->wait = 0;

        tmp_node = tmp_node_next;
    }

    tmp_node->wait = 0;

    SFENCE();

#ifdef __sun__
    schedctl_stop(schedctl);
#endif

    return cur_node->ret;
}

static void do_liblock_init_library(ccsynch)()
{}

static void do_liblock_kill_library(ccsynch)()
{}

static void do_liblock_run(ccsynch)(void (*callback)())
{
    if(__sync_bool_compare_and_swap(&liblock_start_server_threads_by_hand,
                                    1, 0) != 1)
        fatal("servers are not managed manually");
    if(callback)
        callback();
}

static int do_liblock_cond_init(ccsynch)(liblock_cond_t *cond)
{
    fatal("not implemented");
}

static int do_liblock_cond_timedwait(ccsynch)(liblock_cond_t *cond,
                                              liblock_lock_t *lock,
                                              const struct timespec *ts)
{
    fatal("not implemented");
}

static int do_liblock_cond_wait(ccsynch)(liblock_cond_t *cond,
                                         liblock_lock_t *lock)
{
    fatal("not implemented");
}

static int do_liblock_cond_signal(ccsynch)(liblock_cond_t *cond)
{
    fatal("not implemented");
}

static int do_liblock_cond_broadcast(ccsynch)(liblock_cond_t *cond)
{
    fatal("not implemented");
}

static int do_liblock_cond_destroy(ccsynch)(liblock_cond_t *cond)
{
    fatal("not implemented");
}

static void do_liblock_on_thread_start(ccsynch)(struct thread_descriptor *desc)
{
#ifdef __sun__
    schedctl = schedctl_init();
#endif
}

static void do_liblock_on_thread_exit(ccsynch)(struct thread_descriptor *desc)
{
    // TODO: Free local data
}

static void do_liblock_unlock_in_cs(ccsynch)(liblock_lock_t *lock)
{
    fatal("not implemented");
}

static void do_liblock_relock_in_cs(ccsynch)(liblock_lock_t *lock)
{
    fatal("not implemented");
}

static void do_liblock_declare_server(ccsynch)(struct hw_thread *core)
{}

static void do_liblock_cleanup(ccsynch)(void)
{
    // All mmap'd/malloc'd variables seem to be released cleanly for this lock.
}

liblock_declare(ccsynch);

