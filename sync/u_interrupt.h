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

#define DUI(x)	asm volatile("%0x06 %[arg]",  : [arg] "=r" (x))
#define EUI(x)	asm volatile("%0x07 %[arg]",  : [arg] "=r" (x))
#define SENDI(x, y)	asm volatile("%0x16 %[arg] %[arg2]",  : [arg] "=r" (x) : [arg2] "=r" (y))
#define POLL() 

#else

#define DUI(x)	dui(x)
#define EUI(x) 	eui(x)
#define SENDI(x, y) sendI(x, y)
#define POLL() poll()

#endif

void dui(int flag);
void eui(int flag);

void sendI(message *msg, int target);

void i_handler(int core_idx);
void poll();

#endif
