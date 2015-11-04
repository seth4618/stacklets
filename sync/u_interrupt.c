#include "u_interrupt.h"
#include "spinlock.h"
#include "queue.h"
#include "system.h"
#include <pthread.h>

void init() {
	int i = 0;
	for (i = 0; i < NUM_CORES; ++i)
	{
		init_queue(msg_bufs[i]);
		flags[i] = 0;
	}
}

void DUI(int core_idx) {
	flags[core_idx] = 1;
}

void EUI(int core_idx) {
	flags[core_idx] = 0;
	poll(core_idx);
}

void sendI(callback_t callback, int target, void* p) {
	message *msg = (message *)malloc(sizeof(message));
	if (msg == NULL)
	{
		return;
	}
	msg -> callback = callback;
	msg -> p = p;
    printf("Thread %lu enqueues message on %d\n",
            (unsigned long)pthread_self(), target);
	enqueue(msg_bufs[target], msg); 
}

void i_handler(int core_idx) {
	queue *q = msg_bufs[core_idx];
	while(!is_empty(q)) {
        printf("Thread id %lu inspects q\n",(unsigned long)pthread_self());
	    message *msg = dequeue(q);
	    callback_t c = msg -> callback;
	    (*c)(msg -> p);
	    free(msg);
	}
	assert(is_empty(q));
}

void poll(int core_idx) {
	if (flags[core_idx] == 1)
	{
		return;
	} 
	else {
		i_handler(core_idx);
    }

}
