#ifndef _SPINLOCK_H
#define _SPINLOCK_H

#define SPINLOCK_LOCKED 1
#define SPINLOCK_UNLOCKED 0


typedef struct spinlock {
    int state;
} spinlock_t;

void spinlock_init(spinlock_t *sl);
void spinlock_lock(spinlock_t *sl);
void spinlock_unlock(spinlock_t *sl);
void spinlock_destroy(spinlock_t *sl);

#endif /* _SPINLOCK_H */
