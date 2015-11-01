#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sched.h>
#include "u_interrupt.h"


struct mystruct {
    int my_id;
    bool trigger;
};

struct lock {
    int owner_id;
    int waiter;   
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

// Returns -1 on error
static int get_myid()
{
    unsigned cpu;

    cpu = sched_getcpu();
    return cpu;
}

void mylock(struct lock *L)
{
    int my_id  = get_myid();
    bool *my_trigger;
    struct addme_cfd cfd;

    if (my_id == -1) {
        fprintf(stderr,"ERROR: mylock: Failed to get cpu id\n");
        return;
    }

    my_trigger = get_trigger(my_id);
    
    if (L == NULL) {
        fprintf(stderr, "ERROR: mylock: Lock not allocated! \n");
        return;
    }

    if (L->owner_id == -1)
retry:        
        L->owner_id = my_id;

    //fence();

    if ((L->owner_id != my_id)) {
        if (L->owner_id == -1) { // It just got freed
            goto retry;
        } else {
            cfd.L = L;
            cfd.ask_id = my_id;
            sendI(&addme, L->owner_id, &cfd);  /* Our instruction */
            while(!(*my_trigger));
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

    for (i=0; i<NUM_CORES; i++) {
        pcpu[i].my_id = i;
        pcpu[i].trigger = 0;
    }

    L = malloc(sizeof(struct lock));       
    L->owner_id = -1;
    L->waiter = -1;
}
