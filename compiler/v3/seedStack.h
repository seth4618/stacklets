// @file seedStack.h
//
// seed stack
#pragma once

typedef struct Seed {
    void* adr;
    void* sp;
    struct Seed* prev;
    struct Seed* next;
    int id;
    int pusherId;
    int stackId;
} Seed;

// must be called by MAIN thread (AND ONLY MAIN THREAD)
void seedStackInit(int numThreads);

// allocate and initialize a seed
Seed* initSeed(void* adr, void* sp);

// push an initialized seed onto tid's stack.  if lock==1, grab lock first
void pushSeed(Seed* seed, int tid, int lock);

// pop top seed from tid's stack.  if unlock==1, release lock afterwards
void popSeed(int tid, int unlock);

// get and return top seed from tid's stack.  DON'T change ptrs if
// returning a non-null seed, then lock for tid's stack will be
// grabbed. Otherwise not
Seed* peekSeed(int tid);

// release lock on tid's seedStack
void seedStackUnlock(int tid);

// grab lock on tid's seedStack
void seedStackLock(int tid);


// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
