// @file stacklet.c

#include "stacklet.h"
#include "stdio.h"
#include "seedStack.h"
#include "readyQ.h"
#include "debug.h"
#include <stdlib.h>
#include "myfib.h"
#include <assert.h>
#include "tracker.h"

__thread void* systemStack;
__thread int threadId;
int numberOfThreads;

pthread_mutex_t readyQLock;

// there is no stacklet stub for the main stack

void*
systemStackInit()
{
    void* systemStackBuffer;
    systemStackBuffer = calloc(1, SYSTEM_STACK_SIZE);
    systemStack = systemStackBuffer + SYSTEM_STACK_SIZE;
    DEBUG_PRINT("systemStack at %p.\n", systemStack);
    return systemStack;
}

void
lockInit()
{
    assert(pthread_mutex_init(&readyQLock, NULL) == 0);
}

void
stackletInit(int numThreads)
{
    numberOfThreads = numThreads;
    lockInit();
    seedStackInit(numThreads);
    readyQInit();
#ifdef TRACKER
    trackerInit(numThreads);
#endif
}

// running at the base of the stacklet to return to its parent.
void
stubRoutine()
{
    Stub* stackletStub;
    getStackletStub(stackletStub);
    void* buf = (char *)stackletStub + sizeof(Stub) - STACKLET_SIZE;
    DEBUG_PRINT("Free a stacklet.\n");
 
    switchToSysStackAndFreeAndResume(buf, stackletStub->parentSP,
            stackletStub->parentPC);
}

// Create a new stacklet to run the seed which we got from tid.
// seedStack of tid is currently locked on entry, but must be released
void
stackletFork(void* parentPC, void* parentSP, void (*func)(void*), void* arg, int tid)
{
    // We can only unlock here because we cannot make function in
    // "SecondChildSteal"
    DPL("sfork from %p:%p@%d\n", parentSP, parentPC, tid);
    seedStackUnlock(tid);
    DEBUG_PRINT("Forking a stacklet.\n");
    void* stackletBuf = calloc(1, STACKLET_SIZE);
    DEBUG_PRINT("\tAllocate stackletBuf %p\n", stackletBuf); //XXX crash here
    Stub* stackletStub = (Stub *)((char *)stackletBuf + STACKLET_SIZE - sizeof(Stub));

    stackletStub->parentSP = parentSP;
    stackletStub->parentPC = parentPC;
    stackletStub->stubRoutine = stubRoutine;

    switchAndJmpWithArg(stackletStub, func, arg);
}

// Only return to caller if no seed available.
// If available, we will cleanup and start executing seed
static void
getSeedIfAvailableFrom(int tid)
{
    // look at seedStack.  If there is stuff there, grab it
    Seed* seed = peekSeed(tid);
    if (seed != NULL) {
	// remember, lock for my seedStack has been grabbed!
	void* adr = seed->adr;
	void* sp = seed->sp;
	releaseSeed(seed, tid);
	DPL(">%p:%p(%d)\n", sp, adr, tid);
	switchAndJmp(sp, adr, tid);
    }
}

// Context switch to another user thread in readyQ (explicit seed) or in seed
// stack (implicity seed).
//
// The current user thread will be lost if it is not put into the readyQ before
// calling this function. This funciton is executed in a separate system stack.
void
suspend()
{
#ifdef TRACKER
    trackingInfo[threadId]->suspend++;
#endif

    dprintLine("suspend\n");
    for (;;)
    {
	// first look in my seedQ.  If there is work there, grab it and go
	getSeedIfAvailableFrom(threadId);
	
	// Now, look in global readyQ.  If there is work there, grab it and go
        ReadyThread* ready = readyDummyHead->front;
        if (ready != NULL)
        {
            assert(0);
            void* adr = ready->adr;
            void* sp = ready->sp;
            deqReadyQ();
            switchAndJmp(sp, adr, -1);
        }

	// No easily available work.  So randomly search for work from other seedQs.  If you find one, grab it and Go.
	// Try 3 times, then check other stuff
	int i;
	for (i=0; i<3; i++) {
	    int x = (threadId+i)%numberOfThreads; /* this should be a random number */
	    getSeedIfAvailableFrom(x);
	}
    }
}

// Yield to another thread.
void
yield(void)
{
    labelhack(Resume);

    DEBUG_PRINT("Put self in readyQ and suspend\n");
    Registers saveArea;
    saveRegisters();
    void* stackPointer;
    getStackPointer(stackPointer);
    enqReadyQ(&&Resume, stackPointer);

    suspendStub();

Resume:
    restoreRegisters();
    DEBUG_PRINT("Resumed a ready thread.\n");
}


// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
