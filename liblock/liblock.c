/* ########################################################################## */
/* (C) UPMC, 2010-2011                                                        */
/*     Authors:                                                               */
/*       Jean-Pierre Lozi <jean-pierre.lozi@lip6.fr>                          */
/*       Gaël Thomas <gael.thomas@lip6.fr>                                   */
/*       Florian David <florian.david@lip6.fr>                                */
/*       Julia Lawall <julia.lawall@lip6.fr>                                  */
/*       Gilles Muller <gilles.muller@lip6.fr>                                */
/* -------------------------------------------------------------------------- */
/* ########################################################################## */
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#ifdef __linux__
#include <numaif.h>
#elif defined(__sun__)
#include <sys/pset.h>
#endif

#include "liblock.h"
#include "liblock-fatal.h"

/*
#define MAX_NUMBER_OF_CORES   1024
#define MAX_NUMBER_OF_THREADS 256*1024
*/
#define MAX_NUMBER_OF_CORES   128
#define MAX_NUMBER_OF_THREADS 1024

/*
#ifndef DEBUG
#define DEBUG
#endif
*/
#ifdef __linux__

#define GET_NODES_CMD                                                          \
    "NODES=/sys/devices/system/node;"                                          \
    "CPUS=/sys/devices/system/cpu;"                                            \
    "if [ -d $NODES ]; then"                                                   \
    "  for F in `ls -d $NODES/node*`; do "                                     \
    "    if [ $(cat $F/cpulist | grep '-') ]; then "                           \
    "      seq -s ' ' $(cat $F/cpulist | tr -s '-' ' '); "                     \
    "    else"                                                                 \
    "      cat $F/cpulist | tr -s ',' ' ';"                                    \
    "    fi; "                                                                 \
    "  done;"                                                                  \
    "else"                                                                     \
    "  find $CPUS -maxdepth 1 -name \"cpu[0-9]*\" -exec basename {} \\; | "    \
    "  sed -e 's/cpu\\(.*\\)/\\1/g' | tr -s '\\n' ' ';"                        \
    "fi"

// FIXME: We don't support hyperthreading on Linux, i.e. we suppose that each
// core has exactly one hardware thread (this will only affect the topology
// structure, not the behavior of the liblock). Supporting hyperthreading
// shouldn't be hard but we don't have a machine with hyperthreading to test it.
#define GET_CORES_CMD GET_NODES_CMD " | tr -s ' ' '\n'"

#define GET_FREQUENCIES_CMD \
    "cat /proc/cpuinfo | grep \"cpu MHz\" | sed -e 's/cpu MHz\\t\\t://'"

#elif defined(__sun__)

#define GET_NODES_CMD                                                          \
   "for COND in `psrinfo -pv | "                                               \
   "             sed -n 's/[^(]*(\\(.*\\)-\\(.*\\))$/i=\\1;i\\<\\=\\2/p'`; do" \
   "  nawk 'BEGIN{ first=1; for('$COND';i++) "                                 \
   "                    { printf \"%s%s\",(first?\"\":\" \"),i; first=0;}}'; " \
   "  echo;"                                                                   \
   "done"

#define GET_CORES_CMD                                                          \
   "exec bash -c '"                                                            \
   "HW_THREAD_IDS=( $(kstat cpu_info | grep -E \"instance\""                   \
   "                                 | awk \"{print \\$4}\") );"               \
   "CORE_IDS=( $(kstat cpu_info | grep -E \"core_id\" "                        \
   "                            | awk \"{print \\$2}\") );"                    \
   "i=0;"                                                                      \
   "last_id=${CORE_IDS[0]};"                                                   \
   "while [[ $i -lt 128 ]]; do"                                                \
   "    if [[ $last_id -ne ${CORE_IDS[$i]} ]]; then"                           \
   "        last_id=${CORE_IDS[$i]};"                                          \
   "        echo;"                                                             \
   "    elif [[ $i -ne 0 ]]; then"                                             \
   "        echo -n \" \";"                                                    \
   "    fi;"                                                                   \
   "    echo -n \"${HW_THREAD_IDS[$i]}\";"                                     \
   "    (( i++ ));"                                                            \
   "done;"                                                                     \
   "'"

#define GET_FREQUENCIES_CMD \
   "kstat cpu_info | grep 'clock_MHz' | sed -n 's/[^0-9]*\\([0-9]*\\).*/\\1/p'"

#endif


struct liblock_info {
    struct liblock_info* next;
    const char*          name;
    struct liblock_lib*  liblock;
};

struct start_routine {
    void*             (*start_routine)(void*);
    struct hw_thread* hw_thread;
    void*             arg;
    const char*       server_type;
};

static struct liblock_info*       liblocks = 0;

__thread struct thread_descriptor self = { 0, 0 };

struct id_manager                 id_manager;
struct topology                   real_topology;
struct topology*                  topology = &real_topology;
#ifdef __linux__
static cpu_set_t                  client_cpu_set;
#endif
int                               liblock_start_server_threads_by_hand = 0;
int                               liblock_servers_always_up = 1;
unsigned int                      do_cycle_count = 1;


__attribute__ ((weak))
void liblock_auto_bind() {}
__attribute__ ((weak))
void liblock_on_server_thread_start(const char* lib, unsigned int thread_id) {}
__attribute__ ((weak))
void liblock_on_server_thread_end(const char* lib, unsigned int thread_id) {}


inline void* liblock_allocate(size_t n)
{
    void* res = 0;
    if((MEMALIGN(&res, CACHE_LINE_SIZE, cache_align(n)) < 0) || !res)
        fatal("MEMALIGN(%llu, %llu)",
              (unsigned long long)n, (unsigned long long)cache_align(n));
    return res;
}

void* anon_mmap(size_t n)
{
    void* res = mmap(0, n, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE,
                     -1, 0);

    if(res == MAP_FAILED)
        fatal("mmap(%d): %s", (int)n, strerror(errno));

    return res;
}

void* anon_mmap_huge(size_t n)
{
#ifdef __linux__
    void* res = mmap(0, n, PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1, 0);

    if(res == MAP_FAILED)
        fatal("mmap(huge)(%d): %s", (int)n, strerror(errno));

    return res;
#elif defined(__sun__)
    // fatal("Large page allocation not implemented (Solaris).");
    // TODO: align on page boundary.
    void* res = mmap(0, n, PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if(res == MAP_FAILED)
        fatal("mmap(huge)(%d): %s", (int)n, strerror(errno));

    return res;
#endif
}

void liblock_bind_mem(void* area, size_t n, struct numa_node* node)
{
#ifdef __linux__
    unsigned long mask = 1 << node->node_id;

    if(topology->nb_nodes > 1)
        if(mbind(area, n, MPOL_BIND, &mask, 1 + topology->nb_nodes,
                 MPOL_MF_MOVE) < 0)
            fatal("mbind: %s", strerror(errno));
#elif defined(__sun__)
//  fprintf(stderr, "Warning: memory not bound (Solaris).\n");
#endif
}

static void extract_topology(const char* cmd_nodes, const char* cmd_cores,
                             const char* cmd_frequencies)
{
    FILE *file;
    char text[1024], *p, *saveptr;
    int hw_threads_per_nodes[1024];
    int hw_threads_per_cores[1024];
    int i;
    int nb_nodes = 0, nb_cores = 0;
    int nb_hw_threads = 0;

    const char* ld = getenv("LD_PRELOAD");

    unsetenv("LD_PRELOAD");

    /* ===================================================================== */
    /* Nodes                                                                 */
    /* ===================================================================== */
    if(!(file = popen(cmd_nodes, "r")))
        fatal("popen");

    while(fgets(text, 1024, file))
    {
        for(i=0, p = strtok_r(text, " ", &saveptr);
            p;
            p = strtok_r(0, " ", &saveptr))
            i++;

        hw_threads_per_nodes[nb_nodes] = i;

        nb_nodes++;
        nb_hw_threads += i;
    }

    if (pclose(file) < 0)
        fatal("pclose");

    topology->nodes = liblock_allocate(nb_nodes * sizeof(struct numa_node));
    topology->hw_threads = liblock_allocate(nb_hw_threads *
                                            sizeof(struct hw_thread));
    topology->nb_nodes = nb_nodes;
    topology->nb_hw_threads = nb_hw_threads;

    for(i=0; i < nb_hw_threads; i++)
    {
#ifdef __linux__
        CPU_SET(i, &client_cpu_set);
#elif defined(__sun__)
        // Nothing to do here
#endif
        topology->hw_threads[i].hw_thread_id = i;
        topology->hw_threads[i].server_type = 0;
    }

    if(!(file = popen(cmd_nodes, "r")))
        fatal("popen");

    nb_nodes = 0;

    while(fgets(text, 1024, file))
    {
        struct numa_node* node = &topology->nodes[nb_nodes];

        node->nb_hw_threads = hw_threads_per_nodes[nb_nodes];
        node->hw_threads = liblock_allocate(node->nb_hw_threads *
                                            sizeof(struct numa_node*));
        node->node_id = nb_nodes++;

        for(i = 0, p = strtok_r(text, " ", &saveptr);
            p;
            p = strtok_r(0, " ", &saveptr), i++)
        {
            struct hw_thread* hw_thread = &topology->hw_threads[atoi(p)];
            node->hw_threads[i] = hw_thread;
            hw_thread->node = node;
        }
    }

    if (pclose(file) < 0)
        fatal("pclose");

    /* ===================================================================== */
    /* Cores                                                                 */
    /* ===================================================================== */
    if(!(file = popen(cmd_cores, "r")))
        fatal("popen");

    while(fgets(text, 1024, file))
    {
        for(i=0, p = strtok_r(text, " ", &saveptr);
            p;
            p = strtok_r(0, " ", &saveptr))
            i++;

        hw_threads_per_cores[nb_cores] = i;
        nb_cores++;
    }

    if (pclose(file) < 0)
        fatal("pclose");

    topology->cores = liblock_allocate(nb_cores * sizeof(struct core));
    topology->nb_cores = nb_cores;

    if(!(file = popen(cmd_cores, "r")))
        fatal("popen");

    nb_cores = 0;

    while(fgets(text, 1024, file))
    {
        struct core* core = &topology->cores[nb_cores];

        core->nb_hw_threads = hw_threads_per_cores[nb_cores];
        core->hw_threads = liblock_allocate(core->nb_hw_threads *
                                            sizeof(struct core*));
        core->core_id = nb_cores++;

        for(i = 0, p = strtok_r(text, " ", &saveptr);
            p;
            p = strtok_r(0, " ", &saveptr), i++)
        {
            struct hw_thread* hw_thread = &topology->hw_threads[atoi(p)];
            core->hw_threads[i] = hw_thread;
            hw_thread->core = core;
        }
    }

    if (pclose(file) < 0)
        fatal("pclose");

    /* ===================================================================== */
    /* Frequencies                                                           */
    /* ===================================================================== */
    file = popen(cmd_frequencies, "r");

    if (file == NULL)
        fatal("popen");

    nb_hw_threads = 0;

    while (fgets(text, 1024, file) != NULL)
        topology->hw_threads[nb_hw_threads++].frequency = atof(text);

    if (pclose(file) < 0)
        fatal("pclose");

    if(ld)
        setenv("LD_PRELOAD", ld, 1);

//  print_topology();
}

void print_topology(void)
{
    int i, j;

    printf("-------------- Frequencies --------------\n");
    for(i=0; i < topology->nb_hw_threads; i++)
    {
        printf("  * Core %d: %f MHz\n",
               topology->hw_threads[i].hw_thread_id,
               topology->hw_threads[i].frequency);
    }

    printf("------------------ Nodes ----------------\n");
    for(i=0; i < topology->nb_nodes; i++)
    {
        printf("  * Node %d:", topology->nodes[i].node_id);
        for(j=0; j<topology->nodes[i].nb_hw_threads; j++)
            printf(" %d (n=%d, c=%d)",
                   topology->nodes[i].hw_threads[j]->hw_thread_id,
                   topology->nodes[i].hw_threads[j]->node->node_id,
                   topology->nodes[i].hw_threads[j]->core->core_id);
        printf("\n");
    }

    printf("------------------ Cores ----------------\n");
    for(i=0; i<topology->nb_cores; i++)
    {
        printf("  * Core %d:", topology->cores[i].core_id);
        for(j=0; j < topology->cores[i].nb_hw_threads; j++)
            printf(" %d (n=%d, c=%d)",
                   topology->cores[i].hw_threads[j]->hw_thread_id,
                   topology->cores[i].hw_threads[j]->node->node_id,
                   topology->cores[i].hw_threads[j]->core->core_id);
        printf("\n");
    }
}

void liblock_define_hw_thread(struct hw_thread* hw_thread)
{
    self.running_hw_thread = hw_thread;
}

void liblock_reserve_hw_thread_for(struct hw_thread* hw_thread,
                                   const char* server_type)
{
    if(__sync_val_compare_and_swap(&hw_thread->server_type,
                                   0, server_type) != 0)
    {
        /*
            printf("binding %p to hw_thread %d with server_type %s from %p\n",
                   server_type, hw_thread->hw_thread_id, server_type,
                   (void*)pthread_self());
        */

        if(!server_type || strcmp(hw_thread->server_type, server_type))
            fatal("trying to bind a '%s (%p)' on the '%s (%p)' %d hw_thread",
                  server_type, server_type, hw_thread->server_type,
                  hw_thread->server_type, hw_thread->hw_thread_id);

        /*
            printf("Binding a thread to hw_thread %d\n",
                   hw_thread->hw_thread_id);
        */

    }
    else if(server_type)
    {
#ifdef __linux__
        cpu_set_t baseset;

        CPU_CLR(hw_thread->hw_thread_id, &client_cpu_set);

        pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &baseset);
        sched_setaffinity(getpid(), sizeof(cpu_set_t), &client_cpu_set);
        if(!self.running_hw_thread)
            CPU_CLR(hw_thread->hw_thread_id, &baseset);


        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &baseset);
#elif defined(__sun__)
        // Nothing to do here
#endif

        liblock_lookup(server_type)->declare_server(hw_thread);

        /*
        printf("Binding a server %s to hw_thread %d\n",
               server_type, hw_thread->hw_thread_id);
        */
    }
}

void liblock_bind_thread(pthread_t tid, struct hw_thread* hw_thread,
                         const char* server_type)
{
    /*
        printf("== binding hw_thread %d with thread %p (from %p)\n",
               hw_thread ? hw_thread->hw_thread_id : -1, (void*)tid,
            (void*)pthread_self());
    */

#ifdef __linux__
    cpu_set_t    _cpu_set;
    cpu_set_t*   cpu_set = &_cpu_set;

    if(hw_thread)
    {
        /* We pin this thread to the right hw_thread. */
        liblock_reserve_hw_thread_for(hw_thread, server_type);

        CPU_ZERO(cpu_set);
        CPU_SET(hw_thread->hw_thread_id, cpu_set);
    }
    else if(server_type)
        fatal("should not happen");
    else
        cpu_set = &client_cpu_set;

    if(pthread_setaffinity_np(tid, sizeof(cpu_set_t), cpu_set))
        fatal("pthread_setaffinity_np");
#elif defined(__sun__)
    if(hw_thread)
    {
        /* We pin this thread to the right hw_thread. */
        liblock_reserve_hw_thread_for(hw_thread, server_type);

        if(processor_bind(P_LWPID, P_MYID, hw_thread->hw_thread_id, NULL))
            fatal("processor_bind (%s)", strerror(errno));
    }
    else if(server_type)
        fatal("should not happen");
#endif
}

unsigned int liblock_find_id(struct id_manager* id_manager)
{
    unsigned int cur;

    if(id_manager->fragmented)
    {
        for(cur=id_manager->first; cur<id_manager->first_free; cur++)
            if(__sync_val_compare_and_swap(&id_manager->bitmap[cur], 1, 0) == 1)
                return cur;

        id_manager->fragmented = 0;
    }

    while((cur = id_manager->first) > 0)
        if(__sync_val_compare_and_swap(&id_manager->first, cur, cur-1) == cur)
            return cur-1;

    while((cur = id_manager->first_free) < id_manager->last)
    {
        if(__sync_val_compare_and_swap(&id_manager->first_free,
                                       cur, cur+1) == cur)
            return cur;
    }

    fatal("exhausted client ids...");
    return 0;
}

void liblock_release_id(struct id_manager* id_manager, unsigned int id)
{
    unsigned int cur;

    while(1)
    {
        if((cur = id_manager->first) == id)
        {
            if(__sync_val_compare_and_swap(&id_manager->first,
                                           cur, cur + 1) == cur)
                return;
        }
        else if((cur = id_manager->first_free) == (id - 1))
        {
            if(__sync_val_compare_and_swap(&id_manager->first_free,
                                           cur, cur + 1) == cur)
                return;
        }
        else
        {
            id_manager->bitmap[id] = 1;
            id_manager->fragmented = 1;
            return;
        }
    }
}

void liblock_init_id_manager(struct id_manager* id_manager)
{
    id_manager->first      = 0;
    id_manager->first_free = 0;
    id_manager->fragmented = 0;
    id_manager->last       = MAX_NUMBER_OF_THREADS;
    id_manager->bitmap     = anon_mmap(MAX_NUMBER_OF_THREADS +
                             sizeof(unsigned char));
}

void* liblock_exec(liblock_lock_t* lock, void* (*pending)(void*), void* val)
{
    return lock->lib->_execute_operation(lock, pending, val);
}

static void cleanup_thread(void* arg)
{
    struct liblock_info* cur;

    for(cur=liblocks ; cur != 0 ; cur=cur->next)
            cur->liblock->on_thread_exit(&self);

    liblock_release_id(&id_manager, self.id);
}

static void* my_start_routine(void* arg)
{
    struct start_routine* r = arg;
    struct liblock_info*  cur;
    void* res;

    self.id = liblock_find_id(&id_manager);
    liblock_define_hw_thread(r->hw_thread);

    if(!r->hw_thread)
        liblock_auto_bind();
#if defined(__sun__)
    else if(processor_bind(P_LWPID, P_MYID, r->hw_thread->hw_thread_id, NULL))
        fatal("processor_bind (%s)", strerror(errno));
#endif

    if (r->server_type)
    {
        for(cur=liblocks ; cur != 0 ; cur=cur->next)
        {
            if (!strcmp(cur->name, r->server_type))
            {
                cur->liblock->on_thread_start(&self);
                break;
            }
        }
    }
    else
    {
        for(cur=liblocks ; cur != 0 ; cur=cur->next)
        {
            cur->liblock->on_thread_start(&self);
        }
    }

    pthread_cleanup_push(cleanup_thread, 0);

    /*
    printf("starting routine: %d with hw_thread %d - %d\n",
           self.id,
           self.running_hw_thread ? self.running_hw_thread->hw_thread_id : -1,
           sched_getcpu());
    */
    res = r->start_routine(r->arg);

    // printf("cleaning up %d\n", self.id);

    pthread_cleanup_pop(1);
    free(r);

    // printf("finishing %d\n", self.id);

    return res;
}

int liblock_thread_create_and_bind(struct hw_thread* hw_thread,
                                   const char* server_type,
                                   pthread_t *thread,
                                   const pthread_attr_t *attr,
                                   void *(*start_routine) (void *), void *arg)
{
    struct start_routine* r = liblock_allocate(sizeof(struct start_routine));
    pthread_attr_t other_attr;
    int res;

    if(attr)
        other_attr = *attr;
    else
        pthread_attr_init(&other_attr);

#ifdef __linux__
    cpu_set_t cpu_set;

    if(hw_thread)
    {
        // printf("creating thread on hw_thread %d\n", hw_thread->hw_thread_id);
        CPU_ZERO(&cpu_set);
        CPU_SET(hw_thread->hw_thread_id, &cpu_set);
        pthread_attr_setaffinity_np(&other_attr, sizeof(cpu_set_t), &cpu_set);
#ifdef DEBUG
        printf("Bound client thread to %d\n", hw_thread->hw_thread_id);
#endif
    } else
        pthread_attr_setaffinity_np(&other_attr, sizeof(cpu_set_t),
                                    &client_cpu_set);
#endif

    r->start_routine = start_routine;
    r->hw_thread = hw_thread;
    r->arg  = arg;
    r->server_type = server_type;

    /*
        printf("building thread on %d\n",
               hw_thread ? hw_thread->hw_thread_id : -1);
    */
    res = pthread_create(thread, hw_thread ? &other_attr : attr,
                         my_start_routine, r);


    /*
        printf("building thread %p on %d done\n", (void*)*thread,
               hw_thread ? hw_thread->hw_thread_id : -1);
    */

    if(res)
        fatal("pthread_create: %s", strerror(res));

    return 0;
}

int liblock_thread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg)
{
    liblock_thread_create_and_bind(0, 0, thread, attr, start_routine, arg);
    return 0;
}

int liblock_getmutex_type(pthread_mutexattr_t* attr)
{
    int res;

    if(attr)
    {
        pthread_mutexattr_gettype(attr, &res);
#ifdef __linux__
        switch(res)
        {
            case PTHREAD_MUTEX_FAST_NP:
                res = PTHREAD_MUTEX_NORMAL;
                break;
            case PTHREAD_MUTEX_RECURSIVE_NP:
                res = PTHREAD_MUTEX_RECURSIVE;
                break;
            case PTHREAD_MUTEX_ERRORCHECK_NP:
                res = PTHREAD_MUTEX_ERRORCHECK;
                break;
        }
#endif
    }
    else
        res = PTHREAD_MUTEX_NORMAL;

    return res;
}

void liblock_printlibs(void)
{
    struct liblock_info** cur;
    int k=0;

    for(cur = &liblocks; *cur != 0; cur = &(*cur)->next)
    {
        if(k++ > 0)
            printf(", ");
        printf("%s", (*cur)->name);
    }

    printf("\n");
}

static struct liblock_info** lookup_info(const char* name)
{
    struct liblock_info** cur;

    for(cur=&liblocks; *cur!=0; cur=&(*cur)->next)
        if(!strcmp((*cur)->name, name))
            return cur;

    return 0;
}

struct liblock_lib* liblock_lookup(const char* name)
{
    struct liblock_info** pnode = lookup_info(name);
    struct liblock_lib* res = 0;

    // printf("looking up the '%s' locking library", name);
    if(pnode)
        res = (*pnode)->liblock;
    else
        fatal("unable to find locking library '%s'", name);

    return res;
}

void liblock_construct(const char* name, struct liblock_lib* liblock)
{
    liblock_init_library();

    liblock_register(name, liblock);
    liblock->on_thread_start(&self);
}

int liblock_register(const char* name, struct liblock_lib* liblock)
{
    struct liblock_info *node, *supposed;

    node = liblock_allocate(sizeof(struct liblock_info));
    node->name = name;
    node->liblock = liblock;

    do
    {
        supposed = liblocks;
        node->next = liblocks;
    }
    while(__sync_val_compare_and_swap(&liblocks, supposed, node) != supposed);

    liblock->init_library();

    return 0;
}

int liblock_lock_init(const char* type, struct hw_thread* hw_thread,
                      liblock_lock_t* lock, void* arg)
{
    struct liblock_lib* lib = liblock_lookup(type);

    if(!lib)
        fatal("unable to find lock: %s", type);

    // printf("DEBUG: liblock_lock_init: lock: %p\n", (void*)lock);

    lock->lib  = lib;
    lock->impl = lib->init_lock(lock, hw_thread, arg);

    return lock->impl ? 0 : -1;
}

int liblock_lock_destroy(liblock_lock_t* lock)
{
    return lock->lib->_destroy_lock(lock);
}

int liblock_cond_init(liblock_cond_t* cond, const pthread_condattr_t* attr)
{
    cond->lib = 0;

    if(attr)
    {
        cond->has_attr = 1;
        cond->attr = *attr;
    }
    else
        cond->has_attr = 0;

    return 0;
}

int liblock_cond_signal(liblock_cond_t* cond)
{
    if(cond->lib)
        return cond->lib->_cond_signal(cond);
    else
        return 0;
}

int liblock_cond_broadcast(liblock_cond_t* cond)
{
    if(cond->lib)
        return cond->lib->_cond_broadcast(cond);
    else
        return 0;
}

int liblock_cond_wait(liblock_cond_t* cond, liblock_lock_t* lock)
{
    struct liblock_lib* lib = lock->lib;

    if(__sync_val_compare_and_swap(&cond->lib, 0, lib))
    {
        if(cond->lib != lib)
            fatal("try to dynamically change the type of a cond");
    }
    else
        lib->_cond_init(cond);

    return lib->_cond_wait(cond, lock);
}

int liblock_cond_timedwait(liblock_cond_t* cond, liblock_lock_t* lock,
                           struct timespec* ts)
{
    struct liblock_lib* lib = lock->lib;

    if(__sync_val_compare_and_swap(&cond->lib, 0, lib))
    {
        if(cond->lib != lib)
            fatal("try to dynamically change the type of a cond");
    }
    else
        lib->_cond_init(cond);

    return lib->_cond_timedwait(cond, lock, ts);
}

int liblock_cond_destroy(liblock_cond_t* cond)
{
    if(cond->lib)
        return cond->lib->_cond_destroy(cond);
    else
        return 0;
}

void liblock_cleanup(void)
{
    struct liblock_info* cur;

    for(cur=liblocks; cur!=0; cur=cur->next)
        cur->liblock->cleanup();
}

void liblock_init_library(void)
{
    static int library_initialized = 0;

    if (library_initialized) return;
    library_initialized = 1;

#ifdef __linux__
    CPU_ZERO(&client_cpu_set);
#endif
    extract_topology(GET_NODES_CMD, GET_CORES_CMD, GET_FREQUENCIES_CMD);
    liblock_init_id_manager(&id_manager);
    self.id = liblock_find_id(&id_manager);
}

