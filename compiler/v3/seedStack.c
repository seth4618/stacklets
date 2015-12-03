// @file seedStack.c

#include <stdlib.h>
#include <assert.h>
#include "seedStack.h"
#include "spinlock.h"

// for debugging
static int current_id;

// 1 seed stack per thread
static Seed** seedStacks;

// 1 lock per thread
static pthread_mutex_t* seedStackLocks;


void 
seedStackInit(int numThreads)
{
    seedStacks = calloc(numThreads, sizeof(Seed*));
    seedStackLocks = calloc(numThreads, sizeof(pthread_mutex_t));
    int i;
    for (i=0; i<numThreads; i++) {
	assert(mySpinInitLock(seedStackLocks+i, NULL) == 0);
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
    seed->id = __sync_fetch_and_add(&current_id, 1); // multithread
    return seed;
}

// push this seed on tid's seedStack.  
// If lock, then grab lock first, but don't release it.
void 
pushSeed(Seed* seed, int tid, int lock)
{
    if (lock) seedStackLock(tid);
    Seed* origHead = seedStacks[tid];
    seedStacks[tid] = seed;
    seed->next = origHead;
    seed->prev = NULL;
    if (origHead) origHead->prev = seed;
}

// pop top seed from tid's seed stack.  
// if unlock, then release lock before we return
void 
popSeed(int tid, int unlock, int pop)
{
    Seed* origHead = seedStacks[tid];
    if (origHead == NULL) {
	dprintLine("lock %d, pop %d, popping on SS@%d, but nothing there?\n", tid, pop, tid);
	assert(0);
    }
    assert(origHead != NULL);
    Seed* next = origHead->next;
    seedStacks[tid] = next;
    if (next != NULL) next->prev = NULL;
    free(origHead);
    if (unlock) seedStackUnlock(tid);
}

// release the seed, free up its memory, etc.
// assume seedStack for this tid is locked.
void
releaseSeed(Seed* seed, int tid)
{
    Seed* prev = seed->prev;
    Seed* next = seed->next;
    if (prev != NULL) prev->next = next; else seedStacks[tid] = next;
    if (next != NULL) next->prev = prev;
    free(seed);
}


// get and return top seed from tid's stack.  DON'T change ptrs if
// returning a non-null seed, then lock for tid's stack will be
// grabbed. Otherwise not
Seed*
peekSeed(int tid)
{
    if (seedStacks[tid] == NULL) return NULL;
    seedStackLock(tid);
    Seed* seed = seedStacks[tid];
    if (seed == NULL) seedStackUnlock(tid);
    return seed;
}

// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
