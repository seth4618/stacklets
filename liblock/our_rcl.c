/* ########################################################################## */
/* (C) UPMC, 2010-2011                                                        */
/*     Authors:                                                               */
/*       Jean-Pierre Lozi <jean-pierre.lozi@lip6.fr>                          */
/*       Gaël Thomas <gael.thomas@lip6.fr>                                    */
/*       Florian David <florian.david@lip6.fr>                                */
/*       Julia Lawall <julia.lawall@lip6.fr>                                  */
/*       Gilles Muller <gilles.muller@lip6.fr>                                */
/*     Modified by:                                                           */
/*       Preeti U. Murthy <preetium@andrew.cmu.edu>                           */
/* -------------------------------------------------------------------------- */
/* ########################################################################## */
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "liblock.h"
#include "liblock-fatal.h"
#include "u_interrupt.h"
#include "queue.h"
#include "spinlock.h"

/*
#ifndef DEBUG
#define DEBUG
#endif
*/
struct request {
    /* Lock associated with the request */
    struct liblock_impl *volatile impl;
    /* Argument of the pending request */
    void* volatile                val;
    /* Pending request or null if no pending request */
    void*              (*volatile pending)(void *);
};

struct liblock_impl {
    /* Current request, used to find it in wait, to update in server_loop to
     * save 150 cycles */
    struct request *volatile      cur_request;
    liblock_lock_t                *liblock_lock;
};
struct server {
    /* Always used by the server in non blocked case, also private in this
     * case */
    /* Hw_thread where the server run (read by client) */
    struct hw_thread              *hw_thread;
    /* The request array (read by client) */
    struct request                *requests;

    /* Number of locks attached to this server */
    int volatile                  nb_attached_locks;
#if defined(PROFILING_ON) || defined(DCM_PROFILING_ON)
    struct profiling              profiling;
#endif
};

struct sendI_pckt {
    struct server *server;
    int ask_id;
};

static volatile int stop_poll = 0, poll_started = 0;

/* Array of server (one per hw_thread) */
static struct server **servers = 0;
static pthread_t *thread_array;
static void force_shutdown(void);

static struct liblock_impl* do_liblock_init_lock(our_rcl)(liblock_lock_t* lock,
                                                        struct hw_thread* hw_thread,
                                                        pthread_mutexattr_t* attr)
{
    struct server *server = servers[hw_thread->hw_thread_id];
    struct liblock_impl *impl = liblock_allocate(sizeof(struct liblock_impl));

    lock->r0 = server;
    impl->liblock_lock = lock;

    __sync_fetch_and_add(&server->nb_attached_locks, 1);
    liblock_reserve_hw_thread_for(hw_thread, "our_rcl");
#ifdef DEBUG
    printf("our_rcl_init_lock on %d\n", hw_thread->hw_thread_id);
#endif

    return impl;
}

static int do_liblock_destroy_lock(our_rcl)(liblock_lock_t* lock)
{
    struct server *server = lock->r0;
    // rclprintf(impl->server, "destroying lock %p", lock);

    __sync_sub_and_fetch(&server->nb_attached_locks, 1);
    /* Check for pending CS and free structures */

    return 0;
}

/*Function to spawn a service thread when interrupted by a client core
 */
static void execute_cs(void *p)
{
    struct sendI_pckt *pckt = (struct sendI_pckt *)p;
    struct request *request;
    void *(*pending)(void*);
    struct server *server = pckt->server;
    int ask_id = pckt->ask_id;

#ifdef DEBUG
    printf("Server=%d inside execute_cs serving thread %d\n",
            server->hw_thread->hw_thread_id, ask_id);
#endif
    DUI(1);

    // First service the request for which you got interrupted
    request = &server->requests[ask_id];
    if (request->pending) {
       pending = request->pending;
       request->val = pending(request->val);

       //Let us first deal with straight forward CS
/*
       if (!(*((int *)request->val) == REQ_ABRT))
           request->pending = 0; */
    }
    request->pending = 0;

    // Now service any pending requests
/*    last = &server->requests[id_manager.first_free];

    for(request=&server->requests[id_manager.first];
        request<last;
        request++)
    {
        if(request->pending) {
            pending = request->pending;
            request->val = pending(request->val);
    
            if (!(*((int *)request->val) == REQ_ABRT))
                request->pending = 0;
        }
    } */
    EUI(0xfffe);
}

static void* do_liblock_execute_operation(our_rcl)(liblock_lock_t* lock,
                                                 void* (*pending)(void*),
                                                 void* val)
{
    struct server *server = lock->r0;
    struct request *req;
    message *msg = (message *)malloc(sizeof(message));
    struct sendI_pckt pckt;
    struct liblock_impl *impl = lock->impl;
    //int wait = 0;

    /*
     * If the cs aborts, it can lead to bad consequences
     * Do not run the cs protected by the server owned lock
     * Such cs should only be requested...But for now ignore
     * this. Let us assume straight forward CSs
     * */
    if(self.id == server->hw_thread->hw_thread_id)
    {
        void *res;
 //      fatal("Cannot run cs owned by the server core");
        res = pending(val);
#ifdef DEBUG
        printf("Server thread %d calling execute_operation\n", self.id);
#endif
        return res;
    }

#ifdef DEBUG
    printf("Thread %d calling execute_operation on %d\n",
            self.id, server->hw_thread->hw_thread_id);
#endif
    req = &server->requests[self.id];

    req->val = val;
    req->pending = pending;
    req->impl = impl;
    pckt.server = server;
    pckt.ask_id = self.id;

    msg->callback = &execute_cs;
    msg->p = &pckt;
    SENDI(msg, server->hw_thread->hw_thread_id);

#if 0
    int i=0;

    for(i=0; i<500; i++) {
        PAUSE();

        if(!req->pending)
            return req->val;
    }

    while(req->pending) {
        YIELD();
    }
#else
    while(req->pending)
    {
        /* This is to take care of cases where the server
         * is waiting on another server core to finish
         * its critical section. In this case nobody can
         * notify the server except the client, but it can
         * do so after a while of wait. Ignore this for now
         * */
/*        if (wait > 4)
            sendI(msg, server->hw_thread->hw_thread_id);
            */
#ifdef WITH_YIELD
        YIELD();
#else
        PAUSE();
#endif
/*        wait++;
        assert(wait > 6);*/
    }
#endif
    return req->val;
}

static void do_liblock_init_library(our_rcl)()
{
    int i;

    servers = liblock_allocate(sizeof(struct server *) *
                               topology->nb_hw_threads);

    for(i = 0; i < topology->nb_hw_threads; i++)
    {
        struct hw_thread *hw_thread = &topology->hw_threads[i];
        int          cid = hw_thread->hw_thread_id;
        size_t       request_size = r_align(sizeof(struct request) *
                                            id_manager.last, PAGE_SIZE);
        size_t       server_size  = r_align(sizeof(struct server), PAGE_SIZE);
        void *       ptr = anon_mmap_huge(request_size + server_size);

        ((uint64_t *)ptr)[0] = 0; // To avoid a page fault later.
        //liblock_bind_mem(ptr, request_size + server_size, hw_thread->node);

        servers[cid] = ptr + request_size;
        servers[cid]->hw_thread = hw_thread;
        servers[cid]->requests = ptr;
        servers[cid]->nb_attached_locks = 0;
    }

    thread_array = liblock_allocate(sizeof(pthread_t) *
                        topology->nb_hw_threads);

#ifdef DCM_PROFILING_ON
    if (PAPI_is_initialized() == PAPI_NOT_INITED &&
        PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT)
        fatal("PAPI_library_init");

    PAPI_thread_init((unsigned long (*)(void))pthread_self);
#endif
    init_system();
    atexit(force_shutdown);
#ifdef DEBUG
    printf("our_rcl_init_library\n");
#endif
}

static void do_liblock_kill_library(our_rcl)()
{
    int i;

    for(i = 0; i < topology->nb_hw_threads; i++)
    {
        struct hw_thread *hw_thread = &topology->hw_threads[i];
        int cid = hw_thread->hw_thread_id;

        free(servers[cid]);
    }
}

static void * start_poll(void *arg)
{
    int hw_cpu_id = *((int *)arg);

#ifdef DEBUG
    printf("Thread %d running poll\n", hw_cpu_id);
#endif
    while(!stop_poll) {
        POLL();
    }
    return NULL;
}

static void do_liblock_run(our_rcl)(void (*callback)())
{
    int i, err;
    pthread_attr_t *attr;
    cpu_set_t *cpu_set;

    if(__sync_val_compare_and_swap(&liblock_start_server_threads_by_hand,
                                   1, 0) != 1)
        fatal("servers are not managed by hand");
    if(callback)
        callback();

    attr = liblock_allocate(sizeof(pthread_attr_t) *
                            topology->nb_hw_threads);
    cpu_set = liblock_allocate(sizeof(cpu_set_t) *
                            topology->nb_hw_threads);
    for(i = 0; i < 1; i++) {
        CPU_ZERO(&cpu_set[i]);
        CPU_SET(i, &cpu_set[i]);
        pthread_attr_setaffinity_np(&attr[i], sizeof(cpu_set_t), &cpu_set[i]);
        err = pthread_create(&thread_array[i], &attr[i], start_poll,
                        &(topology->hw_threads[i].hw_thread_id));
        if (err) {
            fprintf(stderr, "Error creating polling threads\n");
            exit(1);
        }
    }
    poll_started = 1;
#ifdef DEBUG
    printf("our_rcl_run\n");
#endif
}

static int do_liblock_cond_init(our_rcl)(liblock_cond_t* cond)
{
    return 0;
}

static int do_liblock_cond_wait(our_rcl)(liblock_cond_t* cond,
                                       liblock_lock_t* lock)
{
    fatal("We must not have waited in a CS");
}

static int do_liblock_cond_timedwait(our_rcl)(liblock_cond_t* cond,
                                            liblock_lock_t* lock,
                                            const struct timespec* ts)
{
    fatal("We must not have waited in a CS");
}

// Nobody waits on any condition when using our_rcl
// Hence nobody to signal
static int do_liblock_cond_signal(our_rcl)(liblock_cond_t* cond)
{
    return 0;
}

static int do_liblock_cond_broadcast(our_rcl)(liblock_cond_t* cond)
{
    return 0;
}

static int do_liblock_cond_destroy(our_rcl)(liblock_cond_t* cond)
{
    return 0;
}

static void do_liblock_on_thread_start(our_rcl)(struct thread_descriptor* desc)
{
    int i;

    for(i = 0; i<topology->nb_hw_threads; i++)
    {
        servers[i]->requests[self.id].pending = 0;
    }

#ifdef PROFILING_ON
    __sync_fetch_and_add(&nb_client_threads, 1);
#endif
}

static void do_liblock_on_thread_exit(our_rcl)(struct thread_descriptor* desc)
{}

static void do_liblock_unlock_in_cs(our_rcl)(liblock_lock_t* lock)
{
    fatal("implement me");
}

static void do_liblock_relock_in_cs(our_rcl)(liblock_lock_t* lock)
{
    fatal("implement me");
}

static void do_liblock_declare_server(our_rcl)(struct hw_thread* core)
{}

static void do_liblock_cleanup(our_rcl)(void)
{
    int i;

    for(i = 0; i < topology->nb_hw_threads; i++)
    {
        struct hw_thread *hw_thread = &topology->hw_threads[i];
        int cid = hw_thread->hw_thread_id;

        free(servers[cid]);
    }
}

static void force_shutdown(void)
{
    if (poll_started) {
#ifdef DEBUG
        printf("our_rcl_force_shutdown\n");
#endif
        int i, err;
        stop_poll = 1;

         for(i = 0; i < 1; i++)
         {
             err = pthread_join(thread_array[i], NULL);
             if (err) {
                 fprintf(stderr, "Error joining threads\n");
                 exit(1);
             }
         }
    } 
    return;
}


liblock_declare(our_rcl);

