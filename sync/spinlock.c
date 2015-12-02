#include "spinlock.h"
#include "atomic_xchange.h"
#include "assert.h"

#define local_val_compare_and_swap(type, ptr, old_val, new_val) ({             \
            type tmp = old_val;                                                \
            asm volatile("\tcmpxchg %3,%1;" /* no lock! */                     \
                         : "=a" (tmp), "=m" (*(ptr))                           \
                         : "0" (tmp), "r" (new_val)                            \
                         : "cc");                                              \
                                                                               \
            tmp;                                                               \
        })
void spinlock_init(spinlock_t *sl)
{
    sl -> state = SPINLOCK_UNLOCKED;
}

void spinlock_lock(spinlock_t *sl)
{
    int is_locked;
    while(local_val_compare_and_swap(int, &is_locked, 0, 1)) continue;
    sl->state = SPINLOCK_LOCKED;
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
