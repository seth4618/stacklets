// @file myfib.c

#include "myfib.h"
#include "stacklet.h"
#include "seedStack.h"
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include "myassert.h"

#ifdef DEBUG
volatile int line;
SpinLockType lineLock;
int lineReturned[300000000];
SpinLockType lineReturnedLock;
#endif

#ifdef BENCHMARK
struct timeval startTime;
struct timeval endTime;
#endif

void
checkStubBase(int where)
{
    void* mysp;
    asm("movq %%rsp, %[var]" : [var] "=r" (mysp));
    void* osb = getOldStubBase(mysp);

    // check that stubBase is correct
    if (stubBase != osb) {
	dprintLine("%d: Subbases not equal!  old:%p  new:%p\n", where, stubBase, osb);
	assert(0);
    }
}


int result[] = {0,1,1,2,3,5,8,13,21,34,55,89,144,233,377,610,987,1597,2584,
                4181,6765,10946,17711,28657,46368,75025,121393,196418,317811,
                514229,832040,1346269,2178309,3524578,5702887,9227465,
                14930352,24157817,39088169,63245986,102334155,165580141,
                267914296,433494437,701408733,1134903170,1836311903};
static int suspCtr = 0;
static int suspHere = 3;

void
fib(void* F)
{
    // This is to prevent the compiler from eliminating the label through
    // deadcode analysis.
    labelhack(FirstChildDoneNormally);
    labelhack(SecondChildSteal);
    labelhack(FirstChildDone);
    labelhack(SecondChildDone);

#ifdef TRACKER
    __sync_add_and_fetch(&trackingInfo.fib, 1);
#endif

    Foo* f = (Foo *)F;
    Registers saveArea; //XXX can this make sure saveArea is on the stack

    void* mysp;
    asm("movq %%rsp, %[var]" : [var] "=r" (mysp));
    //dprintLine(">fib(%d)  at %p\n", f->input, mysp);

    checkStubBase(1);

    if (f->input <= 2)
    {

#ifdef DEBUG
        mySpinLock(&lineLock);
        DEBUG_PRINT("[threadId = %ld, n = %d, line = %d]\n",
            threadId, f->input, line++);
        mySpinUnlock(&lineLock);
#endif

	//dprintLine("<fib(%d) = %d  at %p\n", f->input, 1, mysp);
        f->output = 1;
        return;
    }

#if 0
    if (suspCtr++ >= suspHere) {
        suspCtr = 0;
        yield();
    }
#endif

#ifdef DEBUG
    mySpinLock(&lineLock);
    int volatile localLine = line++;
    DEBUG_PRINT("[threadId = %ld, n = %d, line = %d]\n",
        threadId, f->input, localLine);
    mySpinUnlock(&lineLock);
#endif

    Foo* a = (Foo *)calloc(1, sizeof(Foo));
    Foo* b = (Foo *)calloc(1, sizeof(Foo));
    a->input = f->input - 1;
    b->input = f->input - 2;

#if 0
    // longer execution like this?
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 10;
    nanosleep(&ts, NULL);
#endif

    // stacklet ===========================
    void* stackPointer;
    getStackPointer(stackPointer);
    Seed* seed = initSeed(&&SecondChildSteal, stackPointer);
    pushSeed(seed, threadId, 1);
    int volatile syncCounter = 0; // "volatile" to prevent deadcode elimination,
                                  // and also for synchronization.
    void* volatile firstChildReturnAdr = &&FirstChildDoneNormally;
    void* localStubBase = stubBase; // used more than 12 hours to find this bug...
    int ptid = threadId;
    saveRegisters();
    seedStackUnlock(threadId); // uh.... need to lock until here..
    // ====================================

    fib(a);
    restoreRegisters(); // may not need to as we already used "volatile" on
                        // "firstChildReturnAdr"
    seedStackLock(ptid);
    checkStubBase(2);
    goto *firstChildReturnAdr;

 FirstChildDoneNormally:
    // stacklet ===========================
    popSeed(ptid, 1);	       /* also releases lock */
    // ====================================

    checkStubBase(3);

    fib(b);
    f->output = a->output + b->output;
    //dprintLine("<fib(%d) = %d  at %p\n", f->input, f->output, mysp);
    return;

SecondChildSteal: // We cannot make function calls here!
    // stacklet ===========================
    restoreRegisters();
    //recoverParentTID(ptid);
    firstChildReturnAdr = &&FirstChildDone;
    syncCounter = 2;
    stubBase = localStubBase;	/* SCG:what is local stubbase? */
    checkStubBase(4);
    saveRegisters();
#ifdef TRACKER
    __sync_add_and_fetch(&trackingInfo.fork, 1);
#endif
    stackletForkStub(&&SecondChildDone, stackPointer, fib, (void *)b, ptid);
    // ====================================

FirstChildDone:
    // stacklet ===========================
    dprintLine("fib(%d) has first child returned\n", f->input);
    restoreRegisters();
    seedStackUnlock(ptid);
    {
        int localSyncCounter = __sync_sub_and_fetch(&syncCounter, 1);
        if (localSyncCounter != 0)
        {
            //saveRegisters(); // !!! Cannot save registers here, because the
                               // second child may have already returned.
            suspendStub();
        }
    }
    restoreRegisters();
    // ====================================

    checkStubBase(5);

#ifdef TRACKER
    __sync_add_and_fetch(&trackingInfo.firstReturn, 1);
#endif

#ifdef DEBUG
    mySpinLock(&lineReturnedLock);
    assert(lineReturned[localLine] == 0);
    lineReturned[localLine] = 1;
    mySpinUnlock(&lineReturnedLock);
#endif

    f->output = a->output + b->output;
    return;

SecondChildDone: // We cannot make function calls before we confirm first child
                 // has already returned.
    // stacklet ===========================
    {
        int localSyncCounter = __sync_sub_and_fetch(&syncCounter, 1);
        if (localSyncCounter != 0)
        {
            //saveRegisters(); // same reason as above
            suspendStub();
        }
    }
    restoreRegisters();
    // ====================================

    checkStubBase(6);

#ifdef TRACKER
    __sync_add_and_fetch(&trackingInfo.secondReturn, 1);
#endif

#ifdef DEBUG
    mySpinLock(&lineReturnedLock);
    assert(lineReturned[localLine] == 0);
    lineReturned[localLine] = 1;
    mySpinUnlock(&lineReturnedLock);
#endif

    f->output = a->output + b->output;
    return;
}

void *thread(void *arg)
{
  threadId = (long)arg;
  systemStack = systemStackInit();
  suspend();
  fprintf(stderr, "Got back from suspend!\n");
}

void createPthreads(int numthreads)
{
    pthread_t tid[numthreads];
    long i;
    for (i = 1; i < numthreads; i++)
        pthread_create(&tid[i], NULL, thread, (void*)i);
}

// Start calculating fib.
//
// Currently we are only doing one thread. We can created multiple pthreads here
// to have the calculation run in multithread mode.
int 
startfib(int n, int numthreads)
{
    stackletInit(numthreads);
    createPthreads(numthreads);

#ifdef DEBUG
    mySpinInitLock(&lineLock, NULL);
    mySpinInitLock(&lineReturnedLock, NULL);
#endif

    Registers saveArea;

    Foo* a = (Foo *)calloc(1, sizeof(Foo));
    a->input = n;

    threadId = (long)0; // main thread's id is 0
    systemStack = systemStackInit();

#ifdef BENCHMARK
    gettimeofday(&startTime, NULL);
#endif

    dprintLine("Start first stacklet: %p->input = %d\n", a, a->input);

    // abandon original stack and start running on a stacklet for fib(a)
    firstFork(fib, a);

    dprintLine("Finished with first stacklet: %p->output = %d\n", a, a->output);
    // Only one thread can reach here. It does not have to the main thread;
    return a->output;
}

int
main(int argc, char** argv)
{
    int numthreads = 1;
    if (argc < 2) die("Need at least one argument to run, n."
                      "If 2, then second is # of threads");
    int n = atoi(argv[1]);
    if (n > 46) die("cannot verify if n > 46");
    if (argc == 3) numthreads = atoi(argv[2]);
    printf("*** setup ***\n"
           "Will run fib(%d) on %d thread(s)\n\n", n, numthreads);

    int x = startfib(n, numthreads);
    myassert(x == result[n], "Unexpected Result: %d (should have been %d)\n", x, result[n]);
    printf("*** result ***\n"
           "fib(%d) = %d (verified)\n\n", n, x);

#ifdef TRACKER
    printf("*** tracker info ***\n"
           "called %d times\n"
           "forked %d times\n"
           "first child returns %d times\n"
           "second child returns %d times\n"
           "suspend %d times\n\n",
           trackingInfo.fib, trackingInfo.fork, trackingInfo.firstReturn,
           trackingInfo.secondReturn, trackingInfo.suspend);
#endif

#ifdef BENCHMARK
    gettimeofday(&endTime, NULL);
    double elapsed = (endTime.tv_sec - startTime.tv_sec) +
                     (endTime.tv_usec - startTime.tv_usec) / 1000000.0;
    printf("*** benchmark ***\n"
           "time elapsed: %lf\n\n", elapsed);
#endif

    printf("fib(%d) = %d\n", n, x);
    exit(EXIT_SUCCESS);
}


// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
