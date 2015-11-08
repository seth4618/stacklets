#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sched.h>
#include <pthread.h>
#include "u_interrupt.h"
#include "system.h"
#include "mylock.h"

static pthread_mutex_t l_mutex;
spinlock_t sl;

struct mystruct {
    int my_id;  // Per-cpu id
    long pid;   // Per-process id
    bool trigger;
    int waiter_id;
};


// cfd = call_function_data
// structure that contains arguments to addme call
struct addme_cfd
{
    struct lock *L;
    int ask_id;
};

typedef struct mystruct mystruct_t;
static mystruct_t pcpu[NUM_CORES];

static void addme(void *p);

static bool *get_trigger(int id)
{
    return &pcpu[id].trigger;
}

// Returns -1 on error, returns id >=0 otherwise
static int put_myid(long pid)
{
    int i;

    for (i=0; i<NUM_CORES; i++) {
        if (pcpu[i].pid == -1) {
            pcpu[i].pid= pid;
            return i;
        }
    }
    return -1;
}

//Assert if put_myid() could not find you a slot
//Ideally each pthread must be associated with  cpu
int get_myid()
{
    int i;
    pthread_t pid;

    pid = (long)pthread_self();

    spinlock_lock(&sl);
    for (i=0; i<NUM_CORES; i++) {
        if (pcpu[i].pid == pid)
            goto ret;
    }

    // First time caller
    if ((i = put_myid(pid)) >= 0) {
        goto ret;
    } else {
        spinlock_unlock(&sl);
        assert(0);
    }

ret:
    spinlock_unlock(&sl);
    return i;
}

void mylock(struct lock *L)
{
    int my_id  = get_myid();
    bool *my_trigger;
    struct addme_cfd cfd;

    my_trigger = get_trigger(my_id);
    
    if (L == NULL) {
        fprintf(stderr, "ERROR: mylock: Lock not allocated! \n");
        return;
    }

    if (L->owner_id == -1)
retry:        
        L->owner_id = my_id;

   pthread_mutex_lock(&l_mutex);
   pthread_mutex_unlock(&l_mutex);

    if ((L->owner_id != my_id)) {
        if (L->owner_id == -1) { // It just got freed
            goto retry;
        } else {
            cfd.L = L;
            cfd.ask_id = my_id;
            sendI(&addme, L->owner_id, &cfd);  /* Our instruction */
            printf("Thread id %lu waiting for lock\n",(unsigned long)pthread_self());
            while(!(*my_trigger)) poll(0xFF);
        }
    }
    printf("Thread id %lu got lock\n",(unsigned long)pthread_self());
    *my_trigger = 0;
    // Got the lock
}

// If this routine is called without first calling mylock(), we are screwed
// This library does not take care of such misuse
void myunlock(struct lock *L)
{
    bool *next_trigger;
    int my_id = get_myid();

    if (L == NULL) {
        fprintf(stderr, "ERROR: myunlock: Lock not allocated! \n");
        return;
    }

    DUI(0xFF);
    // No need to disable interrupts since we will not poll in these functions
    if (pcpu[my_id].waiter_id != -1) {
       next_trigger = get_trigger(pcpu[my_id].waiter_id);         
       *next_trigger = 1;
       pcpu[my_id].waiter_id = -1;
    } else {
        L->owner_id = -1;
    }

    printf("Thread id %lu released lock\n",(unsigned long)pthread_self());
    EUI(0xFF);
}

static void addme(void *p)
{
    int my_id = get_myid();
    struct addme_cfd *cfd = (struct addme_cfd *)p;
    struct lock *L = (struct lock *)cfd->L;
    int ask_id = (int)cfd->ask_id;

    if (my_id == -1) {
        fprintf(stderr,"ERROR: mylock: Failed to get cpu id\n");
        return;
    }
    printf("Thread id %lu entering addme\n",(unsigned long)pthread_self());
    DUI(0xFF);  // Our instruction
    if (L->owner_id != my_id) {
        if (L->owner_id == -1) {
            L->owner_id = ask_id;

            //fence();
               pthread_mutex_lock(&l_mutex);
   pthread_mutex_unlock(&l_mutex);


            if (L->owner_id != ask_id) {
                sendI(&addme, L->owner_id, p);
            } else {
                bool *next_trigger = get_trigger(ask_id);
                *next_trigger = 1;
            }
        } else {
            sendI(&addme, L->owner_id, p);
        }
    } else {
        L->owner_id = ask_id;
        pcpu[my_id].waiter_id = ask_id;
    }
    EUI(0xFF); // Our instruction
}

void init_lock(struct lock *L)
{
    int i;

    for (i=0; i<NUM_CORES; i++) {
        pcpu[i].my_id = i;
        pcpu[i].pid = -1;
        pcpu[i].trigger = 0;
        pcpu[i].waiter_id = -1;
    }

    L->owner_id = -1;

}

void destroy_lock(struct lock *L)
{
    int i;

    for (i=0; i<NUM_CORES; i++) {
        pcpu[i].pid = -1;
    }
}
