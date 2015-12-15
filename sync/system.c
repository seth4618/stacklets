#include <stdio.h>
#include <stdlib.h>
#include "u_interrupt.h"
#include "pthread.h"
#include "spinlock.h"
#include "assert.h"
#include "myassert.h"

extern __thread int threadId;
void dprintLine(char* fmt, ...);

struct mystruct {
    int my_id;  // Per-cpu id
    long pid;   // Per-process id
};

typedef struct mystruct mystruct_t;
static mystruct_t *pcpu;

static spinlock_t sl;
// Returns -1 on error, returns id >=0 otherwise
static int put_myid(long pid)
{
    int i;
    int ncpus;

    ncpus = get_nr_cpus();

    for (i = 0; i < ncpus; i++) {
        if (pcpu[i].pid == -1) {
            pcpu[i].pid = pid;
	    dprintLine("Installing %p as %d\n", pid, i);
            return i;
        }
    }
    myassert(0, "Failed to find place to put cpuId for %p\n", pid);
    return -1;
}

//Assert if put_myid() could not find you a slot
//Ideally each pthread must be associated with  cpu
int 
get_myid(void)
{
    int i, ncpus;
    pthread_t pid;

    pid = (long)pthread_self();

    ncpus = get_nr_cpus();

    spinlock_lock(&sl);
    for (i = 0; i < ncpus; i++) {
	if (pcpu[i].pid == pid) goto ret;
    }

    // First time caller
    if ((i = put_myid(pid)) >= 0) {
        goto ret;
    } else {
        spinlock_unlock(&sl);
        assert(0);
    }

 ret:
    myassert(i == threadId, "get_myid returning wrong id:%d, threadId:%d, PID:%p\n", i, threadId, pid);
    spinlock_unlock(&sl);
    return i;
}

void init_pcpu_ids(int ncpus)
{
    int i;

    pcpu = calloc(ncpus, sizeof(mystruct_t));
    myassert(pcpu != NULL, "Cannot allocate memory to pcpu ids\n");

    for (i = 0; i < ncpus; i++) {
        pcpu[i].my_id = i;
        pcpu[i].pid = -1;
    }
    dprintLine("Inited pcpu structure\n");
}



// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
