// @file seedStack.c

#include <stdlib.h>
#include <assert.h>
#include "seedStack.h"
#include "spinlock.h"
#include <stdio.h>
#include "u_interrupt.h"

// for debugging
static int current_id;

// 1 seed stack per thread
static SeedQueue* seedStacks;

#ifndef ULI
// 1 lock per thread
static SpinLockType* seedStackLocks;
#endif

void 
seedStackInit(int numThreads)
{
    seedStacks = calloc(numThreads, sizeof(SeedQueue));
#ifndef ULI
    seedStackLocks = calloc(numThreads, sizeof(SpinLockType));
    int i;
    for (i=0; i<numThreads; i++) {
	mySpinInitLock(seedStackLocks+i);
    }
#endif
}

// release lock on tid's seedStack
void 
seedStackUnlock(int tid)
{
#ifdef ULI
    myassert(tid == threadId, "How can I EUI thread:%d when I am on proc:%d\n", tid, threadId);
    EUI(1);
#else
    mySpinUnlock(seedStackLocks+tid);
#endif
}

// grab lock on tid's seedStack
void 
seedStackLock(int tid)
{
#ifdef ULI
    myassert(tid == threadId, "How can I DUI thread:%d when I am on proc:%d\n", tid, threadId);
    DUI(1);
#else
    mySpinLock(seedStackLocks+tid);
#endif
}

Seed* 
initSeed(void* adr, void* sp)
{
    Seed* seed = calloc(1, sizeof(Seed));
    seed->adr = adr;
    seed->sp = sp;
    //SCG?: Isn't this just for debugging?
    //seed->id = __sync_fetch_and_add(&current_id, 1); // multithread
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
#ifdef ULI
    myassert(tid == threadId, "How can I releaseSeed for thread:%d when I am on proc:%d\n", tid, threadId);
#endif
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
#ifdef ULI
    myassert(tid == threadId, "How can I peekSeed&lock for thread:%d when I am on proc:%d\n", tid, threadId);
#endif
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