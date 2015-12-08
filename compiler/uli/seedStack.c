// @file seedStack.c

#include <stdlib.h>
#include <assert.h>
#include "seedStack.h"
#include "spinlock.h"
#include <stdio.h>

// for debugging
static int current_id;

// 1 seed stack per thread
static SeedQueue* seedStacks;

// 1 lock per thread
static SpinLockType* seedStackLocks;

void 
seedStackInit(int numThreads)
{
    seedStacks = calloc(numThreads, sizeof(SeedQueue));
    seedStackLocks = calloc(numThreads, sizeof(SpinLockType));
    int i;
    for (i=0; i<numThreads; i++) {
	mySpinInitLock(seedStackLocks+i);
    }
}

// release lock on tid's seedStack
void 
seedStackUnlock(int tid)
{
    mySpinUnlock(seedStackLocks+tid);
}

// grab lock on tid's seedStack
void 
seedStackLock(int tid)
{
    mySpinLock(seedStackLocks+tid);
}

Seed* 
initSeed(void* adr, void* sp)
{
    Seed* seed = calloc(1, sizeof(Seed));
    seed->adr = adr;
    seed->sp = sp;
    //SCG?: Isn't this just for debugging?
    seed->id = __sync_fetch_and_add(&current_id, 1); // multithread
    return seed;
}

// push this seed on tid's seedStack.  
// If lock, then grab lock first, but don't release it.
void 
pushSeed(Seed* seed, int tid, int lock)
{
    if (lock) seedStackLock(tid);

    SeedQueue* Q = &seedStacks[tid];
    if (Q->back) {
      Q->back->next = seed;
      seed->prev = Q->back;
      Q->back = seed;
    } else {
      Q->front = seed;
      Q->back = seed;
    }
}

// release the seed, free up its memory, etc.
// assume seedStack for this tid is locked.
void
releaseSeed(Seed* seed, int tid)
{
    SeedQueue* Q = &seedStacks[tid];
    Seed* prev = seed->prev;
    Seed* next = seed->next;
    if (prev != NULL) prev->next = next; else Q->front = next;
    if (next != NULL) next->prev = prev; else Q->back = prev;
    free(seed);
}


// get and return top seed from tid's stack.  DON'T change ptrs if
// returning a non-null seed, then lock for tid's stack will be
// grabbed. Otherwise not
Seed*
peekSeed(int tid)
{
    SeedQueue* Q = &seedStacks[tid];
    if (Q->front == NULL) return NULL;
    seedStackLock(tid);

    if (Q->front == NULL) {
        seedStackUnlock(tid);
        return NULL; // Q->front might have been changed, need to return NULL
    }
    return Q->front;
}

// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
