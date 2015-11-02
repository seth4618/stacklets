#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sched.h>
#include <pthread.h>
#include "u_interrupt.h"
#include "system.h"
#include "mylock.h"


struct mystruct {
    int my_id;  // Per-cpu id
    long pid;   // Per-process id
    bool trigger;
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
    unsigned cpu;
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

    for (i=0; i<NUM_CORES; i++) {
        if (pcpu[i].pid == pid)
            return i;
    }

    // First time caller
    if ((i = put_myid(pid)) >= 0)
        return i;
    else
        assert(1);

}

void mylock(struct lock *L)
{
    int my_id  = get_myid();
    bool *my_trigger;
    struct addme_cfd cfd;

    my_trigger = get_trigger(my_id);
    poll(my_id);
    
    if (L == NULL) {
        fprintf(stderr, "ERROR: mylock: Lock not allocated! \n");
        return;
    }

    poll(my_id);
    if (L->owner_id == -1)
retry:        
        L->owner_id = my_id;

    poll(my_id);
    //fence();

    if ((L->owner_id != my_id)) {
        if (L->owner_id == -1) { // It just got freed
            poll(my_id);
            goto retry;
        } else {
            cfd.L = L;
            cfd.ask_id = my_id;
            sendI(&addme, L->owner_id, &cfd);  /* Our instruction */
            poll(my_id);
            while(!(*my_trigger));
        }
    }
    poll(my_id);
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

    poll(my_id);
    DUI(my_id);
    // No need to disable interrupts since we will not poll in these functions
    if (L->waiter != -1) {
       next_trigger = get_trigger(L->waiter);         
       L->waiter = -1;
       *next_trigger = 1;
    } else {
        L->owner_id = -1;
    }

    EUI(my_id);
    poll(my_id);
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
    poll(my_id);
    DUI(my_id);  // Our instruction
    if (L->owner_id != my_id) {
        if (L->owner_id == -1) {
            L->owner_id = ask_id;

            //fence();

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
        L->waiter = ask_id;
        L->owner_id = ask_id;
    }
    EUI(my_id); // Our instruction
}

void init_lock(struct lock *L)
{
    int i;
    int my_id = get_myid();

    for (i=0; i<NUM_CORES; i++) {
        pcpu[i].my_id = i;
        poll(my_id);
        pcpu[i].pid = -1;
        poll(my_id);
        pcpu[i].trigger = 0;
    }

    poll(my_id);
    L->owner_id = -1;
    poll(my_id);
    L->waiter = -1;
    poll(my_id);
}

void destroy_lock(struct lock *L)
{
    int i;
    int my_id = get_myid();

    for (i=0; i<NUM_CORES; i++) {
        poll(my_id);
        pcpu[i].pid = -1;
        poll(my_id);
    }
}
