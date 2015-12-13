#include "u_interrupt.h"
#include "queue.h"
#include "system.h"
#include <stdlib.h>
#include <pthread.h>

#define SMP_INTFLAG 0x0001
static int nr_cpus = 4;
static int *flags;
static queue** msg_bufs;

static void init_uint() {
	int i = 0;

    flags = calloc(nr_cpus, sizeof(int));

    if (!flags) {
        fprintf(stderr, "Cannot allocate memory to flags\n");
        exit(1);
    }

    msg_bufs = calloc(nr_cpus, sizeof(queue *));
    if (!msg_bufs) {
        fprintf(stderr, "Cannot allocate memory to msg_bufs\n");
        exit(1);
    }

	for (i = 0; i < nr_cpus; ++i)
	{
		queue *q = (queue*)malloc(sizeof(struct queue_t));
		if (q == NULL)
		{
			return;
		}
		msg_bufs[i] = q;
		init_queue(q);
		flags[i] = 0;
	}
}

/* flag: interrupt flag */
void dui(int flag) {
    int core_idx = GETMYID();

	flags[core_idx] |= flag;
}

void eui(int flag) {
    int core_idx = GETMYID();

	flags[core_idx] &= flag;   // clear flag
	POLL();
}

void sendI(message *msg, int target) {
	enqueue(msg_bufs[target], msg); 
}

static void i_handler(int core_idx) {
	queue *q = msg_bufs[core_idx];
	while(!is_empty(q)) {
	    message *msg = dequeue(q);
	    callback_t c = msg -> callback;
	    (*c)(msg -> p);
	    free(msg);
	}

}

void poll(void) {
    int core_idx = GETMYID();

	if (flags[core_idx] & SMP_INTFLAG)
	{
		return;
	} 
	else {
		i_handler(core_idx);
    }

}

void init_uli(int ncpus)
{
    int i;

    if (ncpus > 0)
        nr_cpus = ncpus;

    init_pcpu_ids(nr_cpus);
    init_uint();
}

int get_nr_cpus(void)
{
    return nr_cpus;
}
