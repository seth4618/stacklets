#include "u_interrupt.h"
#include "queue.h"
#include "system.h"
#include <stdlib.h>
#include <pthread.h>
#include "myassert.h"

#define SMP_INTFLAG 0x0001
static int nr_cpus = 4;
static int *flags;
static queue** msg_bufs;

void dprintLine(char* fmt, ...);
static int uliDebug = 0;
extern __thread int threadId;
__thread int internalThreadId;

void
setULIdebugLevel(int x)
{
    uliDebug = x;
}

static void 
init_uint(void) 
{
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

  for (i = 0; i < nr_cpus; ++i) {
    queue *q = (queue*)malloc(sizeof(struct queue_t));
    myassert(q != NULL, "Failed to allocated Q");

    msg_bufs[i] = q;
    init_queue(q);
    flags[i] = 0;
  }
}

/* flag: interrupt flag */
void dui(int flag) {
    int core_idx = GETMYID();
    myassert(core_idx == threadId, "core_idx:%d != threadId:%d\n", core_idx, threadId);

	flags[core_idx] |= flag;
}

void eui(int flag) {
    int core_idx = GETMYID();
    myassert(core_idx == threadId, "core_idx:%d != threadId:%d\n", core_idx, threadId);

	flags[core_idx] &= flag;   // clear flag
	POLL();
}

void 
sendI(message *msg, int target) 
{
    if (uliDebug > 0) {
	dprintLine("sendMsg->%d: buffer:%p inlet:%p(%p)\n", target, msg, msg->callback, msg->p);
	if (target == threadId)
	    dprintLine("self send: %d\n", target);
    }
    enqueue(msg_bufs[target], msg); 
}

static void 
i_handler(int core_idx) 
{
    myassert(core_idx == threadId, "core_idx:%d != threadId:%d\n", core_idx, threadId);
    queue *q = msg_bufs[core_idx];
    while(!is_empty(q)) {
	message *msg = dequeue(q);
	callback_t c = msg -> callback;
	if (uliDebug > 0) {
	    dprintLine("Handling:%p with %p(%p)\n", msg, msg->callback, msg->p);
	    dprintLine("Is this weird?\n");
	}
	(*c)(msg -> p);
	free(msg);
    }

}

void 
poll(void) 
{
    int core_idx = GETMYID();
    myassert(core_idx == threadId, "core_idx:%d != threadId:%d\n", core_idx, threadId);

    if (flags[core_idx] & SMP_INTFLAG)
	return;
    else 
	i_handler(core_idx);
}

void init_uli(int ncpus)
{
    int i;

    if (ncpus > 0) {
	// called before any threads created
        nr_cpus = ncpus;
	init_uint();
    } else {
	// called by each thread before they do anything else
	internalThreadId = threadId;
    }
}

int get_nr_cpus(void)
{
    return nr_cpus;
}

int 
get_myid(void)
{
    return internalThreadId;
}


// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
