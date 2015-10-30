#include "u_interrupt.h"
#include "spinlock_type.h"
#include "linked_list.h"

void init() {
	int i = 0;
	for (i = 0; i < NUM_CORES; ++i)
	{
		list_init(&(msg_bufs[i]));
		spinlock_init(&(buf_locks[i]));
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

void sendI(callback_t callback, int target) {
	
}

void remove_buff() {

}

void i_handler() {

}

void poll() {
	if (flag == 1)
	{
		return
	} 
	else
		i_handler();

}