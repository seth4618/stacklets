#ifndef U_INTERRUPT
#define U_INTERRUPT U_INTERRUPT
#include "linked_list.h"
#include "spinlock_type.h"

typedef void (*callback_t)(int);
const int NUM_CORES 4
static list msg_bufs[NUM_CORES];
static int flags[NUM_CORES];
static spinlock_t buf_locks[NUM_CORES];

typedef struct message_t
{
	callback_t callback;
	int source;
}message;

void init();


void DUI(int core_idx);
void EUI(int core_idx);

void sendI(callback_t callback, int target);
void remove_buff(int core_idx);
void i_handler(int core_idx);
void poll(int core_idx);

#endif