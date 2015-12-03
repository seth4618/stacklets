// @file myfib.c

#include "myfib.h"
#include "stacklet.h"
#include "seedStack.h"
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include "tracker.h"

#ifdef DEBUG
volatile int line;
pthread_mutex_t lineLock;
int lineReturned[300000000];
pthread_mutex_t lineReturnedLock;
#endif

#ifdef BENCHMARK
struct timeval startTime;
struct timeval startTimeAfterInit;
struct timeval endTime;
#endif

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
    trackingInfo[threadId]->fib++;
#endif

    Foo* f = (Foo *)F;
    Registers saveArea; //XXX can this make sure saveArea is on the stack

    DPL(">fib(%d)\n", f->input);

    if (f->input <= 2)
    {

#ifdef DEBUG
        pthread_mutex_lock(&lineLock);
        dprintLine("[n = %d, line = %d]\n", f->input, line++);
        pthread_mutex_unlock(&lineLock);
#endif

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
    pthread_mutex_lock(&lineLock);
    int volatile localLine = line++;
    dprintLine("[n = %d, line = %d]\n", f->input, localLine);
    pthread_mutex_unlock(&lineLock);
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
    int ptid = threadId;
    saveRegisters();
    seedStackUnlock(threadId); // uh.... need to lock until here..
    // ====================================

    fib(a);
    restoreRegisters(); // may not need to as we already used "volatile" on
                        // "firstChildReturnAdr"
    seedStackLock(ptid);
    goto *firstChildReturnAdr;

 FirstChildDoneNormally:
    // stacklet ===========================
    popSeed(ptid, 1, seed->id);	       /* also releases lock */
                                       // ptid not threadId!
    // ====================================

    fib(b);
    f->output = a->output + b->output;
    return;

SecondChildSteal: // We cannot make function calls here!
    // stacklet ===========================
    restoreRegisters();
    firstChildReturnAdr = &&FirstChildDone;
    syncCounter = 2;
    saveRegisters();
#ifdef TRACKER
    trackingInfo[threadId]->fork++;
#endif
    stackletForkStub(&&SecondChildDone, stackPointer, fib, (void *)b, ptid);
    // ====================================

FirstChildDone:
    // stacklet ===========================
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

#ifdef TRACKER
    trackingInfo[threadId]->firstReturn++;
#endif

#ifdef DEBUG
    pthread_mutex_lock(&lineReturnedLock);
    assert(lineReturned[localLine] == 0);
    lineReturned[localLine] = 1;
    pthread_mutex_unlock(&lineReturnedLock);
#endif

//    dprintLine("fib(%d) has first child returned\n", f->input);
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

#ifdef TRACKER
    trackingInfo[threadId]->secondReturn++;
#endif

#ifdef DEBUG
    pthread_mutex_lock(&lineReturnedLock);
    assert(lineReturned[localLine] == 0);
    lineReturned[localLine] = 1;
    pthread_mutex_unlock(&lineReturnedLock);
#endif

//    dprintLine("fib(%d) has second child returned\n", f->input);
    f->output = a->output + b->output;
    return;
}

void *thread(void *arg)
{
  threadId = (long)arg;
  systemStack = systemStackInit();
  suspend();
  fprintf(stderr, "Got back from suspend!\n");
  assert(0);
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
#ifdef BENCHMARK
    gettimeofday(&startTime, NULL);
#endif

    stackletInit(numthreads);
    createPthreads(numthreads);

#ifdef DEBUG
    pthread_mutex_init(&lineLock, NULL);
    pthread_mutex_init(&lineReturnedLock, NULL);
#endif

    Registers saveArea;

    Foo* a = (Foo *)calloc(1, sizeof(Foo));
    a->input = n;

    threadId = (long)0; // main thread's id is 0
    systemStack = systemStackInit();

#ifdef BENCHMARK
    gettimeofday(&startTimeAfterInit, NULL);
#endif

    fib(a);

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
    if (argc >= 3) numthreads = atoi(argv[2]);

#ifndef CLEAN
    printf("*** setup ***\n"
           "Will run fib(%d) on %d thread(s)\n\n", n, numthreads);
#endif

    int x = startfib(n, numthreads);
    assert(x == result[n]);

#ifdef BENCHMARK
    gettimeofday(&endTime, NULL);
    double elapsed = (endTime.tv_sec - startTime.tv_sec) +
                     (endTime.tv_usec - startTime.tv_usec) / 1000000.0;
    double elapsedAfterInit = (endTime.tv_sec - startTimeAfterInit.tv_sec) +
                              (endTime.tv_usec - startTimeAfterInit.tv_usec) / 1000000.0;
    printf("*** benchmark ***\n"
           "time elapsed: %.3lf\n"
           "time elapsed after init: %.3lf\n\n",
           elapsed, elapsedAfterInit);
#endif

#ifndef CLEAN
    printf("*** result ***\n"
           "fib(%d) = %d (verified)\n\n", n, x);
#endif

#ifdef TRACKER
    TrackingInfo* finalTrackingInfo = getFinalTrackingInfo();
    printf("*** tracker info ***\n"
           "called %d times\n"
           "forked %d times\n"
           "first child returns %d times\n"
           "second child returns %d times\n"
           "suspend %d times\n\n",
           finalTrackingInfo->fib, finalTrackingInfo->fork, finalTrackingInfo->firstReturn,
           finalTrackingInfo->secondReturn, finalTrackingInfo->suspend);
#endif

    exit(EXIT_SUCCESS);
}


// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
