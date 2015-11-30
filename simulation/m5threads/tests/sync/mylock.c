#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sched.h>
#include <pthread.h>
#include "u_interrupt.h"
#include "system.h"
#include "mylock.h"

static pthread_mutex_t l_mutex;

struct mylockstruct {
    bool trigger;   // Per-cpu trigger
    int waiter_id;  // my waiter
};

// cfd = call_function_data
// structure that contains arguments to addme call
struct addme_cfd
{
    struct lock *L;
    int ask_id;
};

typedef struct mylockstruct mylockstruct_t;
static mylockstruct_t pcpu_lock_struct[NUM_CORES];

static void addme(void *p);

static bool *get_trigger(int id)
{
    return &pcpu_lock_struct[id].trigger;
}

void mylock(struct lock *L)
{
    int my_id  = get_myid();
    bool *my_trigger;
    struct addme_cfd cfd;
    message *msg = (message *)malloc(sizeof(message));

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
            if (msg == NULL)
	        {
	        	goto retry;
	        }
            cfd.L = L;
            cfd.ask_id = my_id;
	        msg -> callback = &addme;
	        msg -> p = &cfd;
            SENDI(msg, L->owner_id);  /* Our instruction */
            while (!(*my_trigger)) POLL();
        }
    }
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

    DUI(1); /* disable the uint #1 */
    // No need to disable interrupts since we will not poll in these functions
    if (pcpu_lock_struct[my_id].waiter_id != -1) {
        next_trigger = get_trigger(pcpu_lock_struct[my_id].waiter_id);
        *next_trigger = 1;
        pcpu_lock_struct[my_id].waiter_id = -1;
    } else {
        L->owner_id = -1;
    }

    EUI(0xfffe);
}

static void addme(void *p)
{
    int my_id = get_myid();
    struct addme_cfd *cfd = (struct addme_cfd *)p;
    struct lock *L = (struct lock *)cfd->L;
    int ask_id = (int)cfd->ask_id;
    message *msg = (message *)malloc(sizeof(message));

    if (my_id == -1) {
        fprintf(stderr, "ERROR: mylock: Failed to get cpu id\n");
        return;
    }
    DUI(1);  // Our instruction: disable the uint #1
    if (L->owner_id != my_id) {
        if (L->owner_id == -1) {
            L->owner_id = ask_id;

            //fence();
            pthread_mutex_lock(&l_mutex);
            pthread_mutex_unlock(&l_mutex);


            if (L->owner_id != ask_id) {
                if (msg == NULL)
	            {
	              	fprintf(stderr,"ERROR:malloc\n");
                    exit(1);
	            }
                msg->callback = &addme;
                msg->p = p;
                SENDI(msg, L->owner_id);
            } else {
                bool *next_trigger = get_trigger(ask_id);
                *next_trigger = 1;
            }
        } else {
            if (msg == NULL)
	        {
	          	fprintf(stderr,"ERROR:malloc\n");
                exit(1);
	        }
            msg->callback = &addme;
            msg->p = p;
            SENDI(msg, L->owner_id);
        }
    } else {
        L->owner_id = ask_id;
        pcpu_lock_struct[my_id].waiter_id = ask_id;
    }
    EUI(0xfffe); // Our instruction
}

void init_lock(struct lock *L)
{
    int i;

    for (i = 0; i < NUM_CORES; i++) {
        pcpu_lock_struct[i].trigger = 0;
        pcpu_lock_struct[i].waiter_id = -1;
    }
}
