#ifndef MYLOCK_H
#define MYLOCK_H

struct lock {
    int owner_id;
};

void mylock(struct lock *L);
void myunlock(struct lock *L);
void init_lock(struct lock *L);

#endif
