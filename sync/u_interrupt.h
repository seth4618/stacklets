#ifndef U_INTERRUPT
#define U_INTERRUPT U_INTERRUPT
#include "queue.h"
#include "spinlock.h"
#include "assert.h"

#define NUM_CORES 4
static queue* msg_bufs[NUM_CORES];
static int flags[NUM_CORES];



void init();


void DUI(int core_idx);
void EUI(int core_idx);

void sendI(callback_t callback, int target, void* p);

void i_handler(int core_idx);
void poll(int core_idx);

#endif