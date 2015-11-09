#include "u_interrupt.h"
#include "spinlock.h"
#include "queue.h"
#include "system.h"
#include <pthread.h>

void init_uint() {
	int i = 0;
	for (i = 0; i < NUM_CORES; ++i)
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
    int core_idx = get_myid();

	flags[core_idx] |= flag;
}

void eui(int flag) {
    int core_idx = get_myid();

	flags[core_idx] &= 0;   // clear flag
	poll();
}

void sendI(message *msg, int target) {
	enqueue(msg_bufs[target], msg); 
}

void i_handler(int core_idx) {
	queue *q = msg_bufs[core_idx];
	while(!is_empty(q)) {
	    message *msg = dequeue(q);
	    callback_t c = msg -> callback;
	    (*c)(msg -> p);
	    free(msg);
	}

}

void poll(void) {
    int core_idx = get_myid();

	if (flags[core_idx] == 1)
	{
		return;
	} 
	else {
		i_handler(core_idx);
    }

}
