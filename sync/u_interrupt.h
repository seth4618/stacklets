#ifndef U_INTERRUPT
#define U_INTERRUPT
#include "queue.h"
#include "spinlock.h"
#include "assert.h"
#include "system.h"

static queue* msg_bufs[NUM_CORES];
static int flags[NUM_CORES];

void init_uint();

#ifdef SIM

#define DUI(x)	asm volatile("%0x6000 %[arg]",  : [arg] "=r" (x))
#define EUI(x)	asm volatile("%0x6001 %[arg]",  : [arg] "=r" (x))
#define SENDI(x, y)	asm volatile("%0x6002 %[arg] %[arg2]",  : [arg] "=r" (x) : [arg2] "=r" (y))
#define POLL() 

#else

#define DUI(x)	dui(x)
#define EUI(x) 	eui(x)
#define SENDI(x, y) sendI(x, y)
#define POLL() our_poll()

#endif

void dui(int flag);
void eui(int flag);

void sendI(message *msg, int target);

void i_handler(int core_idx);
void our_poll();

#endif
