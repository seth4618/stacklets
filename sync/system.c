#include <stdio.h>
#include "system.h"
#include "u_interrupt.h"
#include "pthread.h"

struct mystruct {
    int my_id;  // Per-cpu id
    long pid;   // Per-process id
};

typedef struct mystruct mystruct_t;
static mystruct_t pcpu[NUM_CORES];

static spinlock_t sl;
// Returns -1 on error, returns id >=0 otherwise
static int put_myid(long pid)
{
    int i;

    for (i = 0; i < NUM_CORES; i++) {
        if (pcpu[i].pid == -1) {
            pcpu[i].pid = pid;
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
    for (i = 0; i < NUM_CORES; i++) {
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

void init_system()
{
    int i;

    for (i = 0; i < NUM_CORES; i++) {
        pcpu[i].my_id = i;
        pcpu[i].pid = -1;
    }
    init_uint();
}
