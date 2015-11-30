#include "spinlock.h"
#include "atomic_xchange.h"
#include "assert.h"

void spinlock_init(spinlock_t *sl)
{
    sl -> state = SPINLOCK_UNLOCKED;
}

void spinlock_lock(spinlock_t *sl)
{
    int is_locked;
    while ((is_locked = atomic_xchange(&(sl->state)))) continue;
}


void spinlock_unlock(spinlock_t *sl)
{
    sl -> state = SPINLOCK_UNLOCKED;
}

void spinlock_destroy(spinlock_t *sl)
{
	// Vanish when trying to destroy a locking spinlock
	if (sl -> state == SPINLOCK_LOCKED) return;
    sl = SPINLOCK_UNLOCKED;
}
