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
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <ucontext.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/time.h>

#ifdef __linux__
#include <numa.h>
#include <linux/futex.h>
#elif defined(__sun__)
#include <schedctl.h>
#include <sys/lwp.h>
#include <libcpc.h>
#endif

#include "liblock.h"
#include "liblock-fatal.h"
#include "fqueue.h"
#include "queue.h"
#include "u_interrupt.h"

#define nop() asm volatile ("nop")


#if (defined(PRECISE_DCM_PROFILING_INSIDE_ON) || defined(PRECISE_DCM_PROFILING_OUTSIDE_ON))
#define DCM_PROFILING_ON
#define PRECISE_DCM_PROFILING_ON
#endif

#ifdef DCM_PROFILING_ON

#include <papi.h>

#ifndef __sun__
#define DCM_PROFILING_EVENT 0x80000002
#else
#define DCM_PROFILING_EVENT "DC_miss"
#endif

#ifndef MONITORED_THREAD_ID
#define MONITORED_THREAD_ID -1
#endif

#endif


/*
 * Constants
 */
static const struct timespec manager_timeout = { 0, 50000000 };

#define PRIO_BACKUP    2
#define PRIO_SERVICING 3
#define PRIO_MANAGER   4

#define SERVER_DOWN     0
#define SERVER_STOPPING 1
#define SERVER_STARTING 2
#define SERVER_UP       3
#define SERVER_IDLE     7

#define STACK_SIZE            r_align(1024 * 1024, PAGE_SIZE)
#define MINI_STACK_SIZE       r_align(64 * 1024, PAGE_SIZE)

/*
 * Structures
 */
struct request {
    /* Lock associated with the request */
    struct liblock_impl *volatile impl;
    /* Argument of the pending request */
    void* volatile                val;
    /* Pending request or null if no pending request */
    void*              (*volatile pending)(void *);
    void* volatile                servicing_owner;
    char                          pad[pad_to_cache_line(4 * sizeof(void *))];
};

struct liblock_impl {
    /* Current request, used to find it in wait, to update in server_loop to
     * save 150 cycles */
    struct request *volatile      cur_request;
    liblock_lock_t                *liblock_lock;
    /* State of the lock */
    int volatile                  locked;
    char                          pad[pad_to_cache_line(sizeof(int) +
                                                        2 * sizeof(void *))];
};

struct native_thread {
    /* Has recently run */
    int volatile                  timestamp;
    /* Currently associated mini thread */
    struct mini_thread *volatile  mini_thread;
    /* Server of the thread */
    struct server                 *server;
    /* Thread id */
    pthread_t                     tid;
    /* Interrupted */
    int                           is_servicing;
#ifdef __sun__
    pthread_mutex_t               is_servicing_mutex;
    pthread_cond_t                is_servicing_cond;
#endif
    /* Initial context of the thread */
    ucontext_t                    initial_context;
    /* Pointer to the stack */
    void                          *stack;
    /* Pointer to next node */
    struct fqueue                 ll;
    /* Next thread */
    struct native_thread          *all_next;
};

struct mini_thread {
    /* Context of the mini thread */
    ucontext_t                    context;
    /* Server of the mini thread, used for broadcast */
    struct server                 *server;
    /* True if timed */
    int volatile                  is_timed;
    /* Deadline, only used when the mini-thread is in a timed wait */
    struct timespec               deadline;
    /* Queue of the mini_thread */
    liblock_cond_t *volatile      wait_on;
    /* Result of the wait (timeout or not) */
    int volatile                  wait_res;
    struct fqueue                 ll_ready;
    struct fqueue                 ll_timed;
    struct fqueue                 ll_all;
    void                          *stack;
};

struct profiling {
    unsigned long long            nb_false;
    unsigned long long            nb_tot;
    unsigned long long            nb_cs;
    unsigned long long            nb_normal_path;
    unsigned long long            nb_slow_path;
    unsigned long long            nb_events;
};

struct server {
    /* Always used by the server in non blocked case, also private in this
     * case */
    /* State of the server (running, starting...) */
    int volatile                  state;
    /* True if native thread are able to make progress */
    int volatile                  alive;
    /* Current timestamp */
    int volatile                  timestamp;
    /* Number of threads and of mini threads that are pendings */
    int volatile                  nb_ready_and_servicing;
    /* Always shared (in read) in non blocked case */
    char                          pad0[pad_to_cache_line(4 * sizeof(int))];
    /* Hw_thread where the server run (read by client) */
    struct hw_thread              *hw_thread;
    /* The request array (read by client) */
    struct request                *requests;
    char                          pad1[pad_to_cache_line(2 * sizeof(void *))];
    /* Used in blocked case, private */
    /* List of all active mini threads */
    struct fqueue *volatile       mini_thread_all;
    /* List all the threads */
    struct native_thread*         all_threads;
    /* List of prepared threads */
    struct fqueue *volatile       prepared_threads;
    /* Number of servicing threads that are not executing critical sections */
    int volatile                  nb_free_threads;
    /* Futex value */
    int                           wakeup;
#ifdef __sun__
    pthread_mutex_t               wakeup_mutex;
    pthread_cond_t                wakeup_cond;
#endif
    /* Next timed wait deadline */
    struct timespec volatile      next_deadline;
    char                          pad2[pad_to_cache_line(3 * sizeof(void *) +
                                                         2 * sizeof(int) +
                                                         sizeof(struct
                                                                timespec))];

    /* Not intensive shared accesses */
    /* Callback called when the server is ready to handle request */
    void                          (*volatile callback)();
    /* Sorted list of mini threads that have timeout */
    struct fqueue * volatile      mini_thread_timed;
    /* List of active mini threads */
    struct fqueue *volatile       mini_thread_ready;
    /* List of sleeping mini threads */
    struct fqueue *volatile       mini_thread_prepared;
    /* Lock for state transition */
    pthread_mutex_t               lock_state;
    /* Condition to wait on state transition */
    pthread_cond_t                cond_state;
    /* Number of locks attached to this server */
    int volatile                  nb_attached_locks;
    char                          pad3[pad_to_cache_line(4 * sizeof(void *) +
                                                             sizeof(int) +
                                                             sizeof(pthread_mutex_t) +
                                                             sizeof(pthread_cond_t))];
#if defined(PROFILING_ON) || defined(DCM_PROFILING_ON)
    struct profiling              profiling;
    char                          pad4[pad_to_cache_line(sizeof(struct profiling))];
#endif
};

struct sendI_pckt {
    struct server *server;
    int self_id;
};

/* Array of server (one per hw_thread) */
static struct server **servers = 0;
/* Fake lock always taken, used in wait to avoid a second call to the request */
static struct liblock_impl fake_impl;
/* (local) Pointer to the the native thread */
static __thread struct native_thread *volatile me;
/* No local CAS? */
int liblock_our_rcl_no_local_cas = 0;


#ifdef PROFILING_ON
static unsigned int nb_client_threads = 0;
#elif defined(DCM_PROFILING_ON)
static __thread int thread_monitored;
static __thread int thread_initialized = 0;
#ifndef __sun__
long long g_monitored_event_id = DCM_PROFILING_EVENT;
static __thread int event_set = PAPI_NULL;
#ifndef PRECISE_DCM_PROFILING_ON
static __thread long long values[1];
#endif
#else
char *g_monitored_event_id = DCM_PROFILING_EVENT;
static cpc_t *rcw_cpc;
static __thread cpc_set_t *cpc_set;
static __thread int cpc_index;
static __thread unsigned long long beginning = 0;
#ifndef PRECISE_DCM_PROFILING_ON
static __thread unsigned long long value;
#endif
#endif
#endif


/*
 * Function prototypes
 */
static void servicing_loop(void);
static void ensure_at_least_one_free_thread(struct server *);

/*
 * Debug
 */
void our_rclprintf(struct server *server, const char *msg, ...)
{
    va_list va;
    char buf[65536];
    char *tmp = buf;

    if(self.running_hw_thread)
        tmp += sprintf(tmp, "[%2d", self.running_hw_thread->hw_thread_id);
    else
        tmp += sprintf(tmp, "[NA");

    tmp += sprintf(tmp, "/%2d/%-2d - ", server->hw_thread->hw_thread_id,
                   self.id);

    switch(server->state)
    {
        case SERVER_DOWN:     tmp += printf(tmp, "    down"); break;
        case SERVER_STOPPING: tmp += printf(tmp, "stopping"); break;
        case SERVER_STARTING: tmp += printf(tmp, "starting"); break;
        case SERVER_UP:       tmp += printf(tmp, "      up"); break;
        case SERVER_IDLE:     tmp += printf(tmp, "    idle"); break;
    }
    tmp += sprintf(tmp, " - %d/%d] - ", server->nb_free_threads,
                   server->nb_ready_and_servicing);
    va_start(va, msg);
    tmp += vsprintf(tmp, msg, va);
    tmp += sprintf(tmp, "\n");

    printf("%s", buf);

    if(server->nb_free_threads < 0)
        fatal("should not happen");
}


#ifdef __sun__
void emt_handler(int sig, siginfo_t *sip, void *arg)
{
    fatal("A hardware counter has overflowed.\n");
}
#endif

#ifdef __linux__
static long sys_futex(void *addr1, int op, int val1, struct timespec *timeout,
                      void *addr2, int val3)
{
    return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}
#endif

static inline void wakeup_manager(struct server *server)
{
    server->wakeup = 1;
#ifdef __linux__
    sys_futex(&server->wakeup, FUTEX_WAKE_PRIVATE, 1, 0, 0, 0);
#elif defined(__sun__)
    pthread_mutex_lock(&server->wakeup_mutex);
    pthread_cond_signal(&server->wakeup_cond);
    pthread_mutex_unlock(&server->wakeup_mutex);
#endif
}

/*
 * time_spec shortcuts
 */
#define ts_lt(ts1, ts2)                                                        \
    (((ts1)->tv_sec < (ts2)->tv_sec) ||                                        \
     (((ts1)->tv_sec == (ts2)->tv_sec) && ((ts1)->tv_nsec < (ts2)->tv_nsec)))

#define ts_le(ts1, ts2)                                                        \
    (((ts1)->tv_sec < (ts2)->tv_sec) ||                                        \
     (((ts1)->tv_sec == (ts2)->tv_sec) && ((ts1)->tv_nsec <= (ts2)->tv_nsec)))

#define ts_add(res, ts1, ts2)                                                  \
    ({                                                                         \
        (res)->tv_sec = (ts1)->tv_sec + (ts2)->tv_sec;                         \
        (res)->tv_nsec = (ts1)->tv_nsec + (ts2)->tv_nsec;                      \
        if((res)->tv_nsec > 1e9) {                                             \
            (res)->tv_nsec -= 1e9;                                             \
            (res)->tv_sec++;                                                   \
        }                                                                      \
    })

#define ts_sub(res, ts1, ts2)                                                  \
    ({                                                                         \
        (res)->tv_sec = (ts1)->tv_sec - (ts2)->tv_sec;                         \
        if((ts1)->tv_nsec < (ts2)->tv_nsec) {                                  \
            (res)->tv_sec--;                                                   \
            (res)->tv_nsec = 1e9 + (ts1)->tv_nsec - (ts2)->tv_nsec;            \
        }   else                                                               \
            (res)->tv_nsec = (ts1)->tv_nsec - (ts2)->tv_nsec;                  \
    })

#define ts_gettimeofday(ts, tz)                                                \
    ({ struct timeval tv; int r = gettimeofday(&tv, tz);                       \
       (ts)->tv_sec = tv.tv_sec; (ts)->tv_nsec = tv.tv_usec*1e3; r; })

#define ts_print(ts) printf("%ld.%9.0ld", (ts)->tv_sec, (ts)->tv_nsec)

#if 0
#define lock_print(server, msg) our_rclprintf(server, msg)
#else
#define lock_print(server, msg)
#endif

#define lock_state(server)   ({                                                \
            lock_print(server, "state locking");                               \
            pthread_mutex_lock(&(server)->lock_state);                         \
            lock_print(server, "state locked"); })
#define unlock_state(server) ({ lock_print(server, "state unlock");            \
                                pthread_mutex_unlock(&(server)->lock_state); })

/*
 * Scheduling
 */
__attribute__((always_inline))
static inline void setprio(pthread_t thread_id, unsigned int prio)
{
    // printf("%d goes to prio %d\n", self.id, prio);
    pthread_setschedprio(thread_id, prio);
    /*
    if(pthread_setschedprio(thread_id, prio))
        fatal("unable to set priority: %s for %d", strerror(errno), self.id);
    */
}


/*
 * Atomic operations on a local hw_thread
 */

/* CAS but without any global synchronization */
#ifdef __linux__
#define local_val_compare_and_swap(type, ptr, old_val, new_val) ({             \
            type tmp = old_val;                                                \
            asm volatile("\tcmpxchg %3,%1;" /* no lock! */                     \
                         : "=a" (tmp), "=m" (*(ptr))                           \
                         : "0" (tmp), "r" (new_val)                            \
                         : "cc");                                              \
                                                                               \
            tmp;                                                               \
        })
#elif defined(__sun__)
__attribute__((always_inline))
inline int internal_cas32(volatile int *ptr, int old, int _new)
{
    __asm__ volatile("cas [%2], %3, %0"                     // instruction
                     : "=&r"(_new)                          // output
                     : "0"(_new), "r"(ptr), "r"(old)        // inputs
//                   : "memory");                           // normal CAS
                     : "cc");                               // local CAS

    return _new;
}

inline uint64_t internal_cas64(volatile uint64_t *ptr, uint64_t old,
                               uint64_t _new)
{
    __asm__ volatile("casx [%2], %3, %0"                    // instruction
                     : "=&r"(_new)                          // output
                     : "0"(_new), "r"(ptr), "r"(old)        // inputs
//                   : "memory");                           // normal CAS
                     : "cc");                               // local CAS
    return _new;
}

#define local_val_compare_and_swap(type, ptr, old_val, new_val)                \
    internal_cas32((int*)ptr, (int)old_val, (int)new_val)
/*
#define local_val_compare_and_swap(type, ptr, old_val, new_val)                \
    internal_cas64((uint64_t*)ptr, (uint64_t)old_val, (uint64_t)new_val)
*/
/*
#define local_val_compare_and_swap(type, ptr, old_val, new_val)                \
    __sync_val_compare_and_swap(ptr, old_val, new_val)
*/
#endif

#ifdef __linux__
/* Atomic add but without any global synchronization */
__attribute__((always_inline))
static inline int local_fetch_and_add(int volatile *ptr, int val)
{
    asm volatile( "\txadd %2, %1;" /* no lock! */
                                : "=r" (val), "=m" (*(ptr))
                                : "0" (val)
                                : "cc"); /* just tremove "memory" to avoid
                                            reload of other addresses */
    return val;
}
#elif defined(__sun__)
__attribute__((always_inline))
static inline int local_fetch_and_add(int volatile *ptr, int val)
{
    return __sync_fetch_and_add(ptr, val);
}
#endif

/* Atomic add but without any global synchronization */
__attribute__((always_inline))
static inline int local_add_and_fetch(int volatile *ptr, int val)
{
    return local_fetch_and_add(ptr, val) + val;
}

/*
 * Mini-threads
 */
/* commut from in to out */
static int mini_thread_lt(struct fqueue *ln, struct fqueue *rn)
{
    struct mini_thread* l = ln->content, *r = rn->content;
    return ts_lt(&l->deadline, &r->deadline);
}

__attribute__((always_inline))
static inline void swap_mini_thread(struct mini_thread *in,
                                    struct mini_thread *out)
{
    // our_rclprintf(in->server, "switching from %p to %p", in, out);
    me->mini_thread = out;
    swapcontext(&in->context, &out->context);
}

static struct mini_thread *allocate_mini_thread(struct server *server)
{
    struct mini_thread *res;

    res = liblock_allocate(sizeof(struct mini_thread));
    res->stack = anon_mmap(STACK_SIZE);
    mprotect(res->stack, PAGE_SIZE, PROT_NONE);

/*
    our_rclprintf(server, "CREATE context %p with stack at %p and size %d",
              res, res->stack, STACK_SIZE);
*/

    getcontext(&res->context);
    res->server = server;
    res->context.uc_link = 0;
    res->context.uc_stack.ss_sp = res->stack;
    res->context.uc_stack.ss_size = STACK_SIZE;
    res->ll_ready.content = res;
    res->ll_timed.content = res;
    res->ll_all.content   = res;

    makecontext(&res->context, (void(*)())servicing_loop, 0);

    fqueue_enqueue(&server->mini_thread_all, &res->ll_all);

    return res;
}

static struct mini_thread* get_ready_mini_thread(struct server* server)
{
    struct fqueue* res;

    res = fqueue_dequeue(&server->mini_thread_ready);

    if(res)
    {
        __sync_fetch_and_sub(&server->nb_ready_and_servicing, 1);
        return res->content;
    }
    else
        return 0;
}

static struct mini_thread *get_or_allocate_mini_thread(struct server* server)
{
    struct mini_thread  *res;
    struct fqueue       *node;

    res = get_ready_mini_thread(server);

    // our_rclprintf(server, "*** get or allocate mini thread: ready is %p", res);
    if(res)
        return res;

    node = fqueue_dequeue(&server->mini_thread_prepared);

    if(node)
        return node->content;
    else
        return allocate_mini_thread(server);
}

static void insert_in_ready_and_remove_from_timed(struct fqueue *node)
{
    struct mini_thread *mini_thread = node->content;
    struct server *server = mini_thread->server;

    /*
        our_rclprintf(server, "++++      reinjecting mini thread: %p",
                  mini_thread);
        ll_print("ready", &server->mini_thread_ready);
    */

    fqueue_remove(&server->mini_thread_timed, &mini_thread->ll_timed, 0);
    fqueue_enqueue(&server->mini_thread_ready, &mini_thread->ll_ready);
    __sync_fetch_and_add(&server->nb_ready_and_servicing, 1);

    /*
        our_rclprintf(server, "++++      reinjecting mini thread: %p done",
                  mini_thread);
        ll_print("ready", &server->mini_thread_ready);
    */
}

/*Function to spawn a service thread when interrupted by a client core
 */
static void execute_cs(void *p)
{
    struct sendI_pckt *pckt = (struct sendI_pckt *)p;
    struct request *request, *last;
    void *(*pending)(void*);
    bool retry = 0;
    struct server *server = pckt->server;
    int self_id = pckt->self_id;

    DUI(1);

    // First service the request for which you got interrupted
    request = &server->requests[self_id];
    if (request->pending) {
       pending = request->pending;
       request->val = pending(request->val);

       if (!(request->val == REQ_ABRT))
           request->pending = 0;
    }

    // Now service any pending requests
    last = &server->requests[id_manager.first_free];

    for(request=&server->requests[id_manager.first];
        request<last;
        request++)
    {
        if(request->pending) {
            pending = request->pending;
            request->val = pending(request->val);
    
            if (!(request->val == REQ_ABRT))
                request->pending = 0;
        }
    }
    EUI(0xfffe);
}

/*
 * Liblock API
 */
/* Execute operation (client side) */
static void *do_liblock_execute_operation(our_rcl)(liblock_lock_t *lock,
                                               void *(*pending)(void *),
                                               void *val)
{
    struct liblock_impl *impl = lock->impl;
    struct server *server = lock->r0;
    message *msg = (message *)malloc(sizeof(message));
    struct sendI_pckt pckt;
    int wait = 0;

    /*
        our_rclprintf(server, "*** sending operation %p::%p for client %d - %p",
                  pending, val, self.id, (void*)pthread_self());
    */

    if(me && self.running_hw_thread == server->hw_thread)
    {
        void *res;

        while(local_val_compare_and_swap(int, &impl->locked, 0, 1))
        {
            /* ^ One of my thread owns the lock. */

            me->timestamp = server->timestamp;

            /* Give a chance to one of our threads to release the lock. */
            YIELD();
        }

        res = pending(val);
        impl->locked = 0; /* I release the lock. */

        return res;
    }

    struct request *req = &server->requests[self.id];

    req->impl = impl;
    req->val = val;
    req->pending = pending;
    pckt.server = server;
    pckt.self_id = self.id;

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
         * do so after a while of wait.
         * */
        if (wait > 4)
            sendI(msg, server->hw_thread->hw_thread_id);
#ifdef WITH_YIELD
        YIELD();
#else
        PAUSE();
#endif
        wait++;
        assert(wait > 6);
    }
#endif
    return req->val;
}

__attribute__ ((noinline))
static int servicing_loop_slow_path(struct server *server, int time)
{
    struct mini_thread *next = get_ready_mini_thread(server), *cur;

    if(next)
    {
        cur = me->mini_thread;

        /* More than one ready mini threads, activate the next one and put the
           running one in the prepared list */

        /*
        our_rclprintf(server, "servicing-loop::elect mini-thread: %p "
                     "(and %p goes to prepared)", next, cur);
        */

        fqueue_enqueue(&server->mini_thread_prepared, &cur->ll_ready);

        /*
        ll_print("prepared", &server->mini_thread_prepared);
        */

        swap_mini_thread(cur, next);

        /*
        our_rclprintf(server, "servicing-loop::mini-thread: %p is up",
                  me->mini_thread);
        */

        time = 0;

    }
    else if(server->nb_free_threads > 0)
    {
        /* More than one free thread, put this thread in the sleeping state (not
           servicing anymore) */

        if(local_val_compare_and_swap(int, &me->is_servicing, 1, 0))
        {
            /*
            our_rclprintf(server, "servicing::gona sleep %p %p %p", me, &me->ll,
                      me->ll.content);
            */
            __sync_fetch_and_sub(&server->nb_ready_and_servicing, 1);

            fqueue_enqueue(&server->prepared_threads, &me->ll);

            // our_rclprintf(server, "servicing-loop::unactivating");

#ifdef __linux__
            sys_futex(&me->is_servicing, FUTEX_WAIT_PRIVATE, 0, 0, 0, 0);
#elif defined(__sun__)
            pthread_mutex_lock(&me->is_servicing_mutex);

            while (!me->is_servicing)
            {
                pthread_cond_wait(&me->is_servicing_cond,
                                  &me->is_servicing_mutex);
            }

            pthread_mutex_unlock(&me->is_servicing_mutex);
#endif

            // our_rclprintf(server, "servicing-loop::activating");

            if(!me->is_servicing)
                fatal("inconsistent futex");

            local_fetch_and_add(&server->nb_free_threads, -1);
        }

        time = 0;
    }
    else {
        /* I have strictly more than one servicing thread and no free threads,
           this one is free, activate the next. */

        /*
        static int z = 0;
        if(!(++z % 200000))
            our_rclprintf(server, "servicing-loop::yield processor");
        */

        local_fetch_and_add(&server->nb_free_threads, 1);

        if(time++ > 1000)
        {
            YIELD(); /* all the threads are busy */
            time = 0;
        }

        local_fetch_and_add(&server->nb_free_threads, -1);
    }

    return time;
}

#ifndef __sun__
#define GET_EVENTS()                                                           \
    if (g_monitored_event_id && thread_monitored)                              \
    {                                                                          \
        PAPI_read(event_set, (long long *)&events_before);                     \
    }

#ifdef PRECISE_DCM_PROFILING_OUTSIDE_ON
#define GET_STORE_EVENTS()                                                     \
    if (g_monitored_event_id && thread_monitored)                              \
    {                                                                          \
        PAPI_read(event_set, (long long *)&events_after);                      \
                                                                               \
        if (was_cs)                                                            \
            profiling.nb_events += events_after - events_before;               \
                                                                               \
        was_cs = 0;                                                            \
    }
#else
#define GET_STORE_EVENTS()                                                     \
    if (g_monitored_event_id && thread_monitored)                              \
    {                                                                          \
        PAPI_read(event_set, (long long *)&events_after);                      \
        profiling.nb_events += events_after - events_before;                   \
    }
#endif
#else // !defined(__sun__)
#define GET_EVENTS()                                                           \
    if (g_monitored_event_id && thread_monitored)                              \
    {                                                                          \
        asm volatile ("rd %%asr17, %0\n" : "=r" (events_before));              \
    }

#ifdef PRECISE_DCM_PROFILING_OUTSIDE_ON
#define GET_STORE_EVENTS()                                                     \
    if (g_monitored_event_id && thread_monitored)                              \
    {                                                                          \
        asm volatile ("rd %%asr17, %0\n" : "=r" (events_after));               \
                                                                               \
        events_before >>= 32;                                                  \
        events_after >>= 32;                                                   \
                                                                               \
        if (was_cs)                                                            \
            profiling.nb_events += events_after - events_before;               \
                                                                               \
        was_cs = 0;                                                            \
    }
#else
#define GET_STORE_EVENTS()                                                     \
    if (g_monitored_event_id && thread_monitored)                              \
    {                                                                          \
        asm volatile ("rd %%asr17, %0\n" : "=r" (events_after));               \
                                                                               \
        events_before >>= 32;                                                  \
        events_after >>= 32;                                                   \
                                                                               \
        profiling.nb_events += events_after - events_before;                   \
    }
#endif
#endif


static void servicing_loop()
{
    struct server *server = me->server;
    void (*callback)() = server->callback;
    int time = 0;
#ifdef PROFILING_ON
    struct profiling profiling = { 0, 0, 0, 0, 0, 0 };
    struct liblock_impl *curr_impl = 0;
    int has_with;
#elif defined(DCM_PROFILING_ON)
    struct profiling profiling = { 0, 0, 0, 0, 0, 0 };
#ifdef PRECISE_DCM_PROFILING_ON
    unsigned long long events_before = 0, events_after;
#ifdef PRECISE_DCM_PROFILING_OUTSIDE_ON
    int was_cs = 0;
#endif
#endif
#endif

    if(callback &&
       __sync_val_compare_and_swap(&server->callback, callback, 0) == callback)
    {
        callback();
    }


    // our_rclprintf(server, "::: start servicing loop %p %x", me->mini_thread,
    //           pthread_self());

    // schedctl_start(schedctl_init());

    if (!liblock_our_rcl_no_local_cas)
    {
        do {
            me->timestamp = server->timestamp;
            server->alive = 1;

            struct request *request, *last;
            void *(*pending)(void*);

#ifdef PROFILING_ON
            has_with = 0;
            profiling.nb_normal_path++;
#endif

            last = &server->requests[id_manager.first_free];

            for(request=&server->requests[id_manager.first];
                request<last;
                request++)
            {
                __builtin_prefetch(request + 1, 1, 0);

#ifdef PRECISE_DCM_PROFILING_OUTSIDE_ON
                GET_EVENTS()
#endif
                if(request->pending) {
                    struct liblock_impl *impl = request->impl;

                    if(!local_val_compare_and_swap(int, &impl->locked, 0, 1))
                    {
                        pending = request->pending;

                        if (pending)
                        {
#ifdef PROFILING_ON
                            profiling.nb_cs++;

                            if(!has_with)
                            {
                                has_with = 1;
                                profiling.nb_tot++;
                            }

                            else if(curr_impl != impl)
                            {
                                profiling.nb_false++;
                                curr_impl = impl;
                            }
#endif

#ifdef DCM_PROFILING_ON
                            if (g_monitored_event_id && thread_monitored)
                            {
                                profiling.nb_cs++;
#ifdef PRECISE_DCM_PROFILING_OUTSIDE_ON
                                was_cs = 1;
#endif
                            }
#endif
                            impl->cur_request = request;

#ifdef PRECISE_DCM_PROFILING_INSIDE_ON
                            GET_EVENTS()
                            request->val = pending(request->val);
                            GET_STORE_EVENTS()
#else
                            request->val = pending(request->val);
#endif

                            request->pending = 0;
                            impl->locked = 0;

                        }

#ifdef PRECISE_DCM_PROFILING_OUTSIDE_ON
            GET_STORE_EVENTS()
#endif

            }

            if(server->nb_ready_and_servicing > 1)
            {
                time = servicing_loop_slow_path(server, time);
#ifdef PROFILING_ON
                profiling.nb_slow_path++;
#endif
            }

        }
        while(0);       // With ULI, we do not need a loop
    }
    else // no_local_cas
    {
        do {
            me->timestamp = server->timestamp;
            server->alive = 1;

            struct request *request, *last;
            void *(*pending)(void*);

#ifdef PROFILING_ON
            has_with = 0;
            profiling.nb_normal_path++;
#endif

            last = &server->requests[id_manager.first_free];

            for(request=&server->requests[id_manager.first];
                request<last;
                request++)
            {
                __builtin_prefetch (request + 1, 1, 0);

                pending = request->pending;

#ifdef PRECISE_DCM_PROFILING_OUTSIDE_ON
                GET_EVENTS()
#endif

                if(pending)
                {
                    struct liblock_impl *impl = request->impl;

#ifdef PROFILING_ON
                    profiling.nb_cs++;

                    if(!has_with)
                    {
                        has_with = 1;
                        profiling.nb_tot++;
                    }

                    if(curr_impl != impl)
                    {
                        profiling.nb_false++;
                        curr_impl = impl;
                    }

#endif

#ifdef DCM_PROFILING_ON
                    if (g_monitored_event_id && thread_monitored)
                    {
                        profiling.nb_cs++;
#ifdef PRECISE_DCM_PROFILING_OUTSIDE_ON
                        was_cs = 1;
#endif
                    }
#endif
                    impl->cur_request = request;

#ifdef PRECISE_DCM_PROFILING_INSIDE_ON
                    GET_EVENTS()
                    request->val = pending(request->val);
                    GET_STORE_EVENTS()
#else
                    request->val = pending(request->val);
#endif

                    request->pending = 0;

                }

#ifdef PRECISE_DCM_PROFILING_OUTSIDE_ON
                GET_STORE_EVENTS()
#endif

            }

            if(server->nb_ready_and_servicing > 1)
            {
                time = servicing_loop_slow_path(server, time);
#ifdef PROFILING_ON
                profiling.nb_slow_path++;
#endif
            }

        }
        while(0);       // With ULI, we do not need a loop
    }


#ifdef PROFILING_ON
    __sync_fetch_and_add(&server->profiling.nb_false,
                         profiling.nb_false);
    __sync_fetch_and_add(&server->profiling.nb_tot,
                         profiling.nb_tot);
    __sync_fetch_and_add(&server->profiling.nb_cs,
                         profiling.nb_cs);
    __sync_fetch_and_add(&server->profiling.nb_normal_path,
                         profiling.nb_normal_path);
    __sync_fetch_and_add(&server->profiling.nb_slow_path,
                         profiling.nb_slow_path);
#elif defined(DCM_PROFILING_ON)
    __sync_fetch_and_add(&server->profiling.nb_cs,
                         profiling.nb_cs);
#ifdef PRECISE_DCM_PROFILING_ON
    __sync_fetch_and_add(&server->profiling.nb_events,
                         profiling.nb_events);
#endif
#endif

    // our_rclprintf(server, "::: end of servicing loop %p", me->mini_thread);

    setcontext(&me->initial_context);
    /*
     * Since we do not loop anymore, the manager
     * thread must differentiate between idle
     * server and blocked server
     * */
    server->state = SERVER_IDLE;
}

#ifdef DCM_PROFILING_ON
unsigned long get_thread_id()
{
    return (unsigned long)me->tid;
}
#endif

static void *servicing_thread(void *arg)
{
    struct native_thread  *native_thread = arg;
    struct server         *server = native_thread->server;

#ifdef DCM_PROFILING_ON
#ifndef __sun__
    g_monitored_event_id = DCM_PROFILING_EVENT;
#else
    g_monitored_event_id = DCM_PROFILING_EVENT;
    struct sigaction act;

    /* We register the overflow handler */
    act.sa_sigaction = emt_handler;
    bzero(&act.sa_mask, sizeof(act.sa_mask));
    act.sa_flags = SA_RESTART | SA_SIGINFO;

    if (sigaction(SIGEMT, &act, NULL) == -1)
        fatal("sigaction() failed");
#endif
#endif


#ifdef PROFILING_ON
    __sync_fetch_and_sub(&nb_client_threads, 1);
#endif

    // our_rclprintf(server, "start: servicing thread %d", self.id);

    me = native_thread;
    me->mini_thread = get_or_allocate_mini_thread(server);

    local_fetch_and_add(&server->nb_free_threads, -1);

#ifdef __sun__
    pthread_mutexattr_t is_servicing_attr;

    pthread_mutexattr_init(&is_servicing_attr);
    pthread_mutexattr_setprotocol(&is_servicing_attr, PTHREAD_PRIO_INHERIT);

    pthread_mutex_init(&me->is_servicing_mutex, &is_servicing_attr);
    pthread_cond_init(&me->is_servicing_cond, NULL);
#endif

    if(server->state == SERVER_UP)
    {

        liblock_on_server_thread_start("our_rcl", self.id);
        getcontext(&me->initial_context);

#ifdef DCM_PROFILING_ON
        thread_monitored = ((MONITORED_THREAD_ID < 0) ||
            (server->hw_thread->hw_thread_id == MONITORED_THREAD_ID));

        if (thread_monitored && !thread_initialized)
        {
            thread_initialized = 1;

#ifndef __sun__
            PAPI_thread_init((unsigned long (*)(void))get_thread_id);

            if (g_monitored_event_id)
            {
                if(PAPI_create_eventset(&event_set) != PAPI_OK)
                    warning("PAPI_create_eventset");
                else if(PAPI_add_event(event_set, g_monitored_event_id)
                        != PAPI_OK)
                    warning("PAPI_add_events");
                else if (PAPI_start(event_set) != PAPI_OK)
                    warning("PAPI_start");
            }
#else
            if (g_monitored_event_id)
            {
                if ((cpc_set = cpc_set_create(rcw_cpc)) == NULL)
                    fatal("cpc_set_create() failed");

                cpc_index =
                    cpc_set_add_request(rcw_cpc, cpc_set,
                                        (char *)g_monitored_event_id, 0,
                                        CPC_COUNT_USER | CPC_COUNT_SYSTEM |
                                        CPC_BIND_EMT_OVF, 0, NULL);

                if (cpc_index < 0)
                    fatal("cpc_set_add_request() failed (event=%s)\n",
                          g_monitored_event_id);

                if (cpc_bind_curlwp(rcw_cpc, cpc_set, 0) < 0)
                    fatal("cpc_bind_curlwp");

                asm volatile ("rd %%asr17, %0\n" : "=r" (beginning));
            }
#endif
        }
#endif

    if(server->state == SERVER_UP)
        setcontext(&me->mini_thread->context);

#if defined(DCM_PROFILING_ON) && !defined(PRECISE_DCM_PROFILING_ON)
    if (g_monitored_event_id && thread_monitored)
    {
#ifndef __sun__
        if(PAPI_stop(event_set, values) != PAPI_OK)
            warning("servicing-loop::PAPI_stop");
        else
            __sync_fetch_and_add(&server->profiling.nb_events,
                                 values[0]);
#else
        asm volatile ("rd %%asr17, %0\n" : "=r" (value));
        beginning >>= 32;
        value >>= 32;
        __sync_fetch_and_add(&server->profiling.nb_events,
                             value - beginning);
#endif
    }
#endif

        liblock_on_server_thread_end("our_rcl", self.id);
    }

    __sync_fetch_and_sub(&server->nb_ready_and_servicing, 1);

    // our_rclprintf(server, "::: quitting serviving-loop %p", pthread_self());

    return 0;
}


static void ensure_at_least_one_free_thread(struct server *server)
{
    // our_rclprintf(server, "ensure at least");

    if(server->nb_free_threads < 1)
    {
        /* Ouch, no more free threads, creates or activates a new one */
        if(server->state >= SERVER_STARTING)
        {
            /*
            our_rclprintf(server, "no more free threads %d",
                      server->nb_free_threads);
            */

            struct fqueue *node = fqueue_dequeue(&server->prepared_threads);
            struct native_thread *elected;

            if(node)
            {
                elected = node->content;
                /*
                our_rclprintf(server, "REACTIVATING servicing thread %p", elected);
                */
            }
            else
            {
                elected = liblock_allocate(sizeof(struct native_thread));
                elected->ll.content = elected;

                elected->stack = anon_mmap(MINI_STACK_SIZE);
                mprotect(elected->stack, PAGE_SIZE, PROT_NONE);

                elected->server = server;

                elected->all_next = server->all_threads;
                server->all_threads = elected;

                /*
                our_rclprintf(server, "CREATE a new servicing thread %p with stack"
                          " at %p (%p %p)", elected, elected->stack,
                          &elected->ll, elected->ll.content);
                */
            }

            elected->timestamp = server->timestamp - 1;
            local_fetch_and_add(&server->nb_free_threads, 1);
            __sync_fetch_and_add(&server->nb_ready_and_servicing, 1);
            elected->is_servicing = 1;

            if(node)
            {
#ifdef __linux__
                sys_futex(&elected->is_servicing, FUTEX_WAKE_PRIVATE,
                          1, 0, 0, 0);
#elif defined(__sun__)
                pthread_mutex_lock(&elected->is_servicing_mutex);
                pthread_cond_signal(&elected->is_servicing_cond);
                pthread_mutex_unlock(&elected->is_servicing_mutex);
#endif
            }
            else
            {
                pthread_attr_t        attr;

                pthread_attr_init(&attr);

#ifndef NO_FIFO
                struct sched_param    param;
                param.sched_priority = PRIO_SERVICING;

                pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
                pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
                pthread_attr_setschedparam(&attr, &param);
#endif

               /*
                our_rclprintf(server, "launching the new servicing thread %p",
                          elected);
                */

                liblock_thread_create_and_bind(server->hw_thread, "our_rcl",
                                               &elected->tid, &attr,
                                               servicing_thread, elected);

                /*
                our_rclprintf(server, "launching of the servicing thread %p done",
                          elected);
                */
            }
        }
    }
    // our_rclprintf(server, "ensure done");
}

#ifndef NO_FIFO
static void *backup_thread(void *arg)
{
    struct server *server = arg;

    // our_rclprintf(server, "start: backup");

    while(server->state >= SERVER_STARTING)
    {
        // our_rclprintf(server, "+++ backup thread is running");
        server->alive = 0;
        wakeup_manager(server);
    }

    // our_rclprintf(server, "+++ quitting backup thread %p", pthread_self());

    return 0;
}
#endif

static void *manager_thread(void *arg)
{
    struct server *server = arg;
    struct native_thread *native_thread, *next;
    struct sched_param param;
    pthread_attr_t attr;
    struct timespec now, deadline;
    int done;
#ifdef PROFILING_ON
    int nb_wakeup = 0;
    int nb_not_alive = 0;

    server->profiling.nb_false = 0;
    server->profiling.nb_tot = 0;
    server->profiling.nb_cs = 0;
    server->profiling.nb_normal_path = 0;
    server->profiling.nb_slow_path = 0;
    server->profiling.nb_events = 0;
#endif

#ifdef __sun__
    pthread_mutexattr_t wakeup_attr;

    pthread_mutexattr_init(&wakeup_attr);
    //pthread_mutexattr_setprotocol(&wakeup_attr, PTHREAD_PRIO_INHERIT);

    pthread_mutex_init(&server->wakeup_mutex, &wakeup_attr);
    pthread_cond_init(&server->wakeup_cond, NULL);
#endif

    // our_rclprintf(server, "start: manager");

    server->alive  = 1;

    lock_state(server);

    /* Make sure that we own the lock when going to FIFO scheduling */
    param.sched_priority = PRIO_MANAGER;

    if(pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) == -1)
        fatal("pthread_setschedparam: %s", strerror(errno));

    param.sched_priority = PRIO_BACKUP;
    pthread_attr_init(&attr);

#ifndef NO_FIFO
    pthread_t backup_tid;

    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &param);

    liblock_thread_create_and_bind(server->hw_thread, "rcl", &backup_tid, &attr,
                                   backup_thread, server);
#endif


    server->state = SERVER_UP;

    ensure_at_least_one_free_thread(server);

    pthread_cond_broadcast(&server->cond_state);

    unlock_state(server);

#ifdef NO_FIFO
    goto endloop;
#endif

    // Examine the last two bits only because the server can be idle too
    while((server->state & 0x03) == SERVER_UP)
    {
        // our_rclprintf(server, "manager is working (%d)", server->alive);
        server->wakeup = 0;

#ifdef PROFILING_ON
        nb_wakeup++;
#endif

        /*
         * A server will go idle only if it has no other
         * clients to service. Else it is activated by a ULI
         * */
        if(!server->alive && (server->state != SERVER_IDLE))
        {
            // our_rclprintf(server, "no more live servicing threads");

#ifdef PROFILING_ON
            nb_not_alive++;
#endif
            ensure_at_least_one_free_thread(server);

            server->alive = 1;
            done = 0;

            while(!done)
            {
                struct native_thread *cur = server->all_threads;
                while(cur)
                {
                    if(cur->is_servicing)
                    {
                        if(done || (cur->timestamp == cur->server->timestamp))
                        {
                            setprio(cur->tid, PRIO_BACKUP);
                            setprio(cur->tid, PRIO_SERVICING);
                        }
                        else
                        {
                            cur->timestamp = server->timestamp;
                            /* set it here because could be in I/O */
                            done = 1;
                        }
                    }
                    cur = cur->all_next;
                }

                if(!done)
                    server->timestamp++;
            }
        }
        else
            server->alive = 0;

        ts_gettimeofday(&now, 0);
        ts_add(&deadline, &now, &manager_timeout);

        done = 0;

        /*
        printf("+++++++++++++++++++++++++++++++++++++\n");
        printf("++++      manager: current time: ");
        ts_print(&now); printf("\n");
        printf("++++      manager: initial deadline: ");
        ts_print(&deadline); printf("\n");
        */
        while(!done)
        {
            struct fqueue *node = server->mini_thread_timed;
            if(node)
            {
                struct mini_thread *cur = node->content;
                // printf("++++      manager: find waiter: %p\n", cur);
                if(ts_le(&cur->deadline, &now))
                {
                    // printf("++++      manager: reinject expired deadline\n");
                    fqueue_remove((struct fqueue**)&cur->wait_on->impl.data,
                                  &cur->ll_ready,
                                  insert_in_ready_and_remove_from_timed);
                }
                else
                {
                    struct timespec ddd = cur->deadline;
                    if(ts_lt(&ddd, &deadline))
                    {
                        /*
                        printf("++++      manager: change deadline to: ");
                        ts_print(&deadline);
                        printf("\n");
                        */

                        deadline = ddd;
                    }
                    done = 1;
                }
            }
            else
                done = 1;
        }
        /*
        printf("++++      manager: next deadline: ");
        ts_print(&deadline); printf("\n");
        */

        server->next_deadline = deadline;

        ts_sub(&deadline, &deadline, &now);

        /*
        printf("++++      manager: next deadline: ");
        ts_print(&deadline); printf("\n");
        */

        // our_rclprintf(server, "manager::sleeping");

#ifdef __linux__
        sys_futex(&server->wakeup, FUTEX_WAIT_PRIVATE, 0, &deadline, 0, 0);
#elif defined(__sun__)
        pthread_mutex_lock(&server->wakeup_mutex);
        // while (!server->wakeup)
        pthread_cond_reltimedwait_np(&server->wakeup_cond,
                                     &server->wakeup_mutex, &deadline);
        // fprintf(stderr, "ouch %d\n", res);
        // pthread_cond_wait(&server->wakeup_cond, &server->wakeup_mutex);
        pthread_mutex_unlock(&server->wakeup_mutex);
#endif
        // our_rclprintf(server, "manager::sleeping done");
        // return;
    }

    // our_rclprintf(server, "unblocking all the servicing threads");
    for(native_thread=server->all_threads;
        native_thread;
        native_thread=native_thread->all_next)
    {
        int state = __sync_lock_test_and_set(&native_thread->is_servicing, 2);

        if(!state)
        {
            local_fetch_and_add(&server->nb_free_threads, 1);

            __sync_fetch_and_add(&server->nb_ready_and_servicing, 1);
#ifdef __linux__
            sys_futex(&native_thread->is_servicing, FUTEX_WAKE_PRIVATE,
                      1, 0, 0, 0);
#elif defined(__sun__)
            pthread_mutex_lock(&native_thread->is_servicing_mutex);
            pthread_cond_signal(&native_thread->is_servicing_cond);
            pthread_mutex_unlock(&native_thread->is_servicing_mutex);
#endif
        }

    }

#ifdef NO_FIFO
endloop:
#endif

    // our_rclprintf(server, "waiting backup");

#ifndef NO_FIFO
    if(pthread_join(backup_tid, 0) != 0)
        fatal("pthread_join");
#endif

    // our_rclprintf(server, "waiting servicing threads");

    for(native_thread = server->all_threads; native_thread; native_thread = next)
    {
        next = native_thread->all_next;
        pthread_join(native_thread->tid, 0);
        munmap(native_thread->stack, MINI_STACK_SIZE);
        free(native_thread);
    }

    // our_rclprintf(server, "freeing mini threads");

    {
        struct fqueue* ll_cur;
        while((ll_cur = fqueue_dequeue(&server->mini_thread_all)))
        {
            struct mini_thread *cur = ll_cur->content;
            munmap(cur->stack, STACK_SIZE);
            free(cur);
        }
    }

    // our_rclprintf(server, "quitting");

    server->mini_thread_timed = 0;
    server->mini_thread_ready = 0;
    server->mini_thread_prepared = 0;

    server->prepared_threads = 0;

#ifdef PROFILING_ON
    fprintf(stderr, "--- manager of hw_thread %d\n",
            server->hw_thread->hw_thread_id);
    fprintf(stderr, "    nb wakeup: %d\n", nb_wakeup);
    fprintf(stderr, "    nb not alive: %d\n", nb_not_alive);
    fprintf(stderr, "    false: %llu, total: %llu, cs: %llu, nb-normal %llu,"
            " nb-slow %llu\n", server->profiling.nb_false,
            server->profiling.nb_tot, server->profiling.nb_cs,
            server->profiling.nb_normal_path, server->profiling.nb_slow_path);
    fprintf(stderr, "    false rate: %lf\n",
            (double)server->profiling.nb_false /
            (double)(server->profiling.nb_cs/*.nb_tot * nb_client_threads*/));
    fprintf(stderr, "    slow path rate: %lf\n",
            (double)server->profiling.nb_slow_path /
            (double)server->profiling.nb_normal_path);
    fprintf(stderr, "    use rate: %lf\n",
            (double)server->profiling.nb_cs /
            (double)(server->profiling.nb_tot * nb_client_threads));
    fprintf(stderr, "    nb_cs: %lf\n",
            (double)server->profiling.nb_cs);
    fprintf(stderr, "    nb_tot: %lf\n",
            (double)server->profiling.nb_tot);
    fprintf(stderr, "    nb_client_threads: %d\n",
            nb_client_threads);
#elif defined(DCM_PROFILING_ON) || defined(PRECISE_DCM_PROFILING_ON)
#ifndef __sun__
    long long g_monitored_event_id;
#else
    char *g_monitored_event_id;
#endif

    fprintf(stderr, "--- manager of hw_thread %d\n",
            server->hw_thread->hw_thread_id);

    g_monitored_event_id = DCM_PROFILING_EVENT;

    if(g_monitored_event_id &&
       ((MONITORED_THREAD_ID < 0) ||
        (server->hw_thread->hw_thread_id == MONITORED_THREAD_ID)))
    {
#ifndef __sun__
        char buf[65536];
        strcpy(buf, "<none>");
        PAPI_event_code_to_name(g_monitored_event_id, buf);
        fprintf(stderr, "    '%s'/cs: %lf\n", buf,
                (double)server->profiling.nb_events /
                (double)server->profiling.nb_cs);
#else
        fprintf(stderr, "    '%s'/cs: %lf\n", g_monitored_event_id,
                (double)server->profiling.nb_events /
                (double)server->profiling.nb_cs);
#endif
    }
#endif

    lock_state(server);
    server->state = SERVER_DOWN;
    pthread_cond_broadcast(&server->cond_state);

    unlock_state(server);

    // our_rclprintf(server, "manager thread over");

    return 0;
}

static void launch_server(struct server *server, void (*callback)(void))
{
    pthread_t tid;

    server->callback = callback;
}

static int do_liblock_cond_signal(our_rcl)(liblock_cond_t *cond)
{
    cond_signal(cond);
    return 0;
}

static int do_liblock_cond_broadcast(our_rcl)(liblock_cond_t *cond)
{
    while(cond_signal(cond));
    return 0;
}

static int do_liblock_cond_timedwait(our_rcl)(liblock_cond_t *cond,
                                          liblock_lock_t *lock,
                                          const struct timespec *ts)
{
    struct liblock_impl *impl = lock->impl;
    struct mini_thread  *cur = me->mini_thread;
    struct server* server = me->server;

    // our_rclprintf(server, "timed wait");

    struct mini_thread  *next = get_or_allocate_mini_thread(server);
    struct request      *request = impl->cur_request;

    /* Prepare meta informations */
    cur->is_timed = ts ? 1 : 0;
    cur->wait_on = cond;
    cur->wait_res = 0;

    /* First, don't re-execute the request */
    request->impl = &fake_impl;

    /* Then, enqueue my request in cond  */
    // our_rclprintf(cur->server, "cond:enqueuing: %p", cur);
    fqueue_enqueue((struct fqueue **)&cond->impl.data, &cur->ll_ready);
    // ll_print("cond->impl.data", (struct mini_thread_ll**)&cond->impl.data);

    /* Release the lock */
    impl->locked = 0;

    if(ts)
    {
        // our_rclprintf(cur->server, "cond:timed: %p", cur);
        cur->deadline = *ts;

        fqueue_ordered_insert(&server->mini_thread_timed, &cur->ll_timed,
                              mini_thread_lt);

        if(ts_lt(ts, &server->next_deadline))
            wakeup_manager(server);
    }

    // our_rclprintf(cur->server, "swapping: me is %p", me);

    /* And finally, jump to the next mini thread */
    swap_mini_thread(cur, next);

    // our_rclprintf(server, "%p mini-thread is running (%d)", cur, impl->locked);

    while(local_val_compare_and_swap(int, &impl->locked, 0, 1))
    { /* one of my threads owns the lock */
        me->timestamp = me->server->timestamp;
        YIELD(); /* give a chance to one of our threads to release the lock */
    }

    // our_rclprintf(cur->server, "relected: me continue %p", me);

    request->impl = impl;

    return cur->wait_res;
}

static int do_liblock_cond_wait(our_rcl)(liblock_cond_t *cond, liblock_lock_t *lock)
{
    return do_liblock_cond_timedwait(our_rcl)(cond, lock, 0);
}

static int do_liblock_cond_init(our_rcl)(liblock_cond_t *cond)
{
    cond->impl.data = 0;
    return 0;
}

static int do_liblock_cond_destroy(our_rcl)(liblock_cond_t *cond)
{
    return 0;
}

static void do_liblock_unlock_in_cs(our_rcl)(liblock_lock_t *lock)
{
    fatal("implement me");
}

static void do_liblock_relock_in_cs(our_rcl)(liblock_lock_t *lock)
{
    fatal("implement me");
}

static struct liblock_impl *do_liblock_init_lock(our_rcl)
                                                (liblock_lock_t *lock,
                                                 struct hw_thread *hw_thread,
                                                 pthread_mutexattr_t *attr)
{
    struct server *server = servers[hw_thread->hw_thread_id];
    struct liblock_impl *impl = liblock_allocate(sizeof(struct liblock_impl));

    lock->r0 = server;

    impl->locked = 0;
    impl->liblock_lock = lock;

    liblock_reserve_hw_thread_for(hw_thread, "our_rcl");

    return impl;
}

static int do_liblock_destroy_lock(our_rcl)(liblock_lock_t *lock)
{
    struct server *server = lock->r0;
    return 0;
}

static void do_liblock_run(our_rcl)(void (*callback)())
{
    int i, n = 0;

    if(__sync_val_compare_and_swap(&liblock_start_server_threads_by_hand, 1, 0)
       != 1)
        fatal("servers are not managed by hand");

    for(i = 0;  i < topology->nb_hw_threads; i++) {
        if(topology->hw_threads[i].server_type &&
           !strcmp(topology->hw_threads[i].server_type, "our_rcl"))
            n++;
    }

    if(!n) callback();

    for(i = 0; i < topology->nb_hw_threads; i++)
    {
        lock_state(servers[i]);

        if(topology->hw_threads[i].server_type &&
           !strcmp(topology->hw_threads[i].server_type, "our_rcl"))
        {
            /*
            our_rclprintf(servers[i], "++++ launching: %d from %d", i,
                      sched_getcpu());
            */

            launch_server(servers[i], --n ? 0 : callback);

            /*
            our_rclprintf(servers[i], "++++ launched: %d from %d", i,
                      sched_getcpu());
            */
        }
        unlock_state(servers[i]);
    }

    // printf("-----------     finishing run    ------------------\n");
}

static void do_liblock_declare_server(our_rcl)(struct hw_thread *hw_thread)
{
    if(!liblock_start_server_threads_by_hand)
    {
        lock_state(servers[hw_thread->hw_thread_id]);
        launch_server(servers[hw_thread->hw_thread_id], 0);
        unlock_state(servers[hw_thread->hw_thread_id]);
    }
}

static void do_liblock_on_thread_exit(our_rcl)(struct thread_descriptor *desc) {}

static void do_liblock_on_thread_start(our_rcl)(struct thread_descriptor *desc)
{
    int i;

    // printf("rcl::thread on hw_thread: %d\n", self.id);

    for(i = 0; i<topology->nb_hw_threads; i++)
    {
        servers[i]->requests[self.id].pending = 0;
    }

#ifdef PROFILING_ON
    __sync_fetch_and_add(&nb_client_threads, 1);
#endif
}

static void force_shutdown()
{
    int i;

    for(i = 0; i < topology->nb_hw_threads; i++)
        destroy_server(servers[i]);
}

void our_sigterm_handler(int signal)
{
    force_shutdown();

    exit(0);
}

static void do_liblock_init_library(our_rcl)()
{
    int i;

    servers = liblock_allocate(sizeof(struct server *) *
                               topology->nb_hw_threads);

    fake_impl.locked = 1;

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

        servers[cid]->state = SERVER_DOWN;
        servers[cid]->nb_attached_locks = 0;
        servers[cid]->nb_free_threads = 0;
        servers[cid]->nb_ready_and_servicing = 0;
        servers[cid]->requests = ptr;

        servers[cid]->mini_thread_all = 0;
        servers[cid]->mini_thread_timed = 0;
        servers[cid]->mini_thread_ready = 0;
        servers[cid]->mini_thread_prepared = 0;

        servers[cid]->all_threads = 0;
        servers[cid]->prepared_threads = 0;

        servers[cid]->timestamp = 1;

        pthread_mutex_init(&servers[cid]->lock_state, 0);
        pthread_cond_init(&servers[cid]->cond_state, 0);
    }

    atexit(force_shutdown);

#ifdef DCM_PROFILING_ON
#ifndef __sun__
    if (PAPI_is_initialized() == PAPI_NOT_INITED &&
        PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT)
        fatal("PAPI_library_init");

    PAPI_thread_init((unsigned long (*)(void))pthread_self);
#else
    if ((rcw_cpc = cpc_open(CPC_VER_CURRENT)) == NULL)
        fatal("libcpc version mismatch");
#endif
#endif

    if (signal(SIGTERM, our_sigterm_handler) == SIG_ERR)
        fatal("can't catch SIGTERM");
}

static void do_liblock_kill_library(our_rcl)()
{
    fatal("implement me");
}

static void do_liblock_cleanup(our_rcl)()
{}

liblock_declare(our_rcl);

