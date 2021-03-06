// @file stacklet.c

#include "stacklet.h"
#include <stdio.h>
#include "seedStack.h"
#include "spinlock.h"
#include "readyQ.h"
#include "debug.h"
#include <stdlib.h>
#include "myfib.h"
#include <assert.h>
#include "myassert.h"
#include "tracker.h"

#include "myfib.h"

__thread void* systemStack;
__thread int threadId;
int numberOfThreads;

SpinLockType readyQLock;

//#define TRACK_STACKLETS
#if defined(TRACK_STACKLETS)

static Stub* sinfos = NULL;
SpinLockType stackletInfoLock;
static int sltsAlloc = 0;
static int sltsDealloc = 0;
static int sltsMismatched = 0;

static void 
initStackletInfo(int nt)
{
    mySpinInitLock(&stackletInfoLock);
}

static void 
addStacklet(int tid, int stid, Stub* stub)
{
    mySpinLock(&stackletInfoLock);
    sltsAlloc++;
    stub->next = sinfos;
    if (sinfos) sinfos->prev = stub;
    sinfos = stub;
    stub->prev = NULL;
    stub->allocatorThread = tid;
    stub->seedThread = stid;
    //dprintLine("Adding Stacklet: %p (%d,%d)\n", stub, tid, stid);
    mySpinUnlock(&stackletInfoLock);
}

static void showStacklets(int grablock);

static void 
removeStacklet(int tid, Stub* stub)
{
    Stub* ptr;
    mySpinLock(&stackletInfoLock);
    for (ptr = sinfos; (ptr != NULL) && (ptr != stub); ptr = ptr->next);
    if (ptr == NULL) {
	dprintLine("Expected to find stub %p from %d  ->%p  %p<-\n", 
		   stub, tid, stub->next, stub->prev);
	showStacklets(0);
	myassert(ptr != NULL, "Expected to find stub %p from %d\n", stub, tid);
	mySpinUnlock(&stackletInfoLock);
	return;
    }
    myassert(ptr != NULL, "Expected to find stub %p from %d\n", stub, tid);
    sltsDealloc++;
    Stub* pp = stub->prev;
    Stub* pn = stub->next;
    if (pp) pp->next = pn; else sinfos = pn;
    if (pn) pn->prev = pp;
    if (stub->allocatorThread != tid) {
	sltsMismatched++;
    }
    mySpinUnlock(&stackletInfoLock);
}

static void
showStacklets(int grablock)
{
    if (grablock) mySpinLock(&stackletInfoLock);
    fprintf(stderr, "Stacklets: Allocated:%d, Deallocated:%d, Mismatched:%d\n", 
	    sltsAlloc, sltsDealloc, sltsMismatched);
    Stub* ptr;
    for (ptr = sinfos; ptr != NULL; ptr = ptr->next) {
	fprintf(stderr, "\t%p: sp:%p @ pc:%p alloc:%d seed:%d\n", ptr, 
		ptr->parentSP, ptr->parentPC, 
		ptr->allocatorThread, ptr->seedThread);
    }
    if (grablock) mySpinUnlock(&stackletInfoLock);
}

void showStackletsSafe(void) 
{
    showStacklets(1);
}

void showStackletsUnsafe(void) {
    showStacklets(0);
}
#else
# define initStackletInfo(nt)
# define addStacklet(x, y, z)
# define removeStacklet(x, y)
void showStackletsSafe(void) {}
void showStackletsUnsafe(void) {}
#endif

void*
systemStackInit()
{
    void* systemStackBuffer;
    systemStackBuffer = calloc(1, SYSTEM_STACK_SIZE);
    systemStack = systemStackBuffer + SYSTEM_STACK_SIZE;
    //dprintLine("systemStack = [%p,%p]\n", systemStack, systemStackBuffer);
    return systemStack;
}

void
lockInit()
{
    setupLocks();
    mySpinInitLock(&readyQLock);
}

void
stackletInit(int numThreads)
{
    // must init locks before anything else
    lockInit();

    numberOfThreads = numThreads;
    initStackletInfo(numThreads);
    seedStackInit(numThreads);
    readyQInit();

#ifdef TRACKER
    trackerInit(numThreads);
#endif
}

// we are on system stack and getting calling stacklet sp in callersp,
// address to free in buf, and stackletStub for resuming parent in
// stackletStub.  Called by stubRoutine.

void
finishStubRoutine(void* buf, Stub* stackletStub, void* callerSP)
{
    //dprintLine("finishing Stub for %p (%p)\n", stackletStub, callerSP);
    removeStacklet(threadId, stackletStub);
    void* parentSP = stackletStub->parentSP;
    void* parentPC = stackletStub->parentPC;
    //DON'T FOR NOW free(buf);
    resumeParent(parentSP, parentPC);
}

// running at the base of the stacklet to return to its parent.
void
stubRoutine()
{
    Stub* stackletStub;
    getStackletStub(stackletStub);
    void* buf = (void*)( (
			  // deal with some pushing of sp
		         ((long long unsigned)stackletStub)+RE_ALIGNMENT_FUDGE+ 	
			 // get from stub ptr to base of memory ptr
		         (sizeof(Stub) - STACKLET_SIZE)		
			 // get to alignment boundary
			 ) & (-1LL<<(STACKLET_BUF_ZEROS))	
		       );
    void* adjustedStub = (Stub *)((char *)buf + STACKLET_SIZE - sizeof(Stub));
    //dprintLine("free a stacklet: %p (OR %p)\n", stackletStub, adjustedStub);
    stackletStub = adjustedStub;
 
    switchToSysStackAndFinishStub(buf, stackletStub);
    // ->parentSP,
    // stackletStub->parentPC, stackletStub->parentStubBase);
}



// Create a new stacklet to run the seed which we got from tid.
// seedStack of tid is currently locked on entry, but must be released
void
stackletFork(void* parentPC, void* parentSP, void (*func)(void*), 
	     void* arg, int tid)
{
    // We can only unlock here because we cannot make function call in
    // "SecondChildSteal"
    DPL("sfork from %p:%p@%d\n", parentSP, parentPC, tid);
    seedStackUnlock(tid);
    dprintLine("Forking fib(%d)\n", ((Foo*)arg)->input);
    void* stackletBuf;
    int en = posix_memalign(&stackletBuf, STACKLET_ALIGNMENT, STACKLET_SIZE);
    myassert(en == 0, "Failed to allocate a stacklet");
    //void* stackletBuf = calloc(1, STACKLET_SIZE);
    Stub* stackletStub = (Stub *)((char *)stackletBuf+STACKLET_SIZE-sizeof(Stub));
    //dprintLine("Forked a stacklet: %p\n", stackletStub);

    stackletStub->parentSP = parentSP;
    stackletStub->parentPC = parentPC;
    stackletStub->stubRoutine = stubRoutine;

    addStacklet(threadId, tid, stackletStub);

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

//    dprintLine("suspend\n");
    for (;;)
    {
	// first look in my seedQ.  If there is work there, grab it and go
	getSeedIfAvailableFrom(threadId);
	
	// Now, look in global readyQ.  If there is work there, grab it and go
        ReadyThread* ready = readyDummyHead->front;
        if (ready != NULL)
        {
            void* adr = ready->adr;
            void* sp = ready->sp;
            deqReadyQ();
            switchAndJmp(sp, adr, -1);
        }

	// No easily available work.  So randomly search for work from other seedQs.  
	// If you find one, grab it and Go.
	// Try 3 times, then check other stuff
	int i;
	for (i=0; i<3; i++) {
	    int x = (threadId+i)%numberOfThreads; // this should be a random number
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


// this allocates a stacklet for the base function and 
// will return the instruction following this.

void 
firstFork(void (*func)(void*), void* arg)
{
    Registers saveArea;

    saveRegisters();
    seedStackLock(0);
    asm volatile("movq $FF%=, %%rdi \n"                  \
	"movq %%rsp, %%rsi \n"			\
	"movq %[Afptr], %%rdx \n"		\
	"movq %[Aarg], %%rcx \n"		\
	"xor %%r8, %%r8 \n"			\
	"call stackletFork \n"			\
	"FF%=: \n"				\
	:					\
	: [Afptr] "r" (func),			\
	  [Aarg] "r" (arg)
		 : "rsi", "rdi", "rdx", "rcx", "r8");
    restoreRegisters();
}

// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
