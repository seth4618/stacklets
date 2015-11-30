// @file myfib.c

#include "myfib.h"
#include "stacklet.h"
#include "seedStack.h"
#include <pthread.h>
#include <assert.h>
#include <unistd.h>

__thread long threadId;

#ifdef DEBUG
volatile int line;
pthread_mutex_t lineLock;
int lineReturned[300000000];
pthread_mutex_t lineReturnedLock;
#endif

void
fib(void* F)
{
    // This is to prevent the compiler from eliminating the label through
    // deadcode analysis.
    labelhack(FirstChildDoneNormally);
    labelhack(SecondChildSteal);
    labelhack(FirstChildDone);
    labelhack(SecondChildDone);

    Foo* f = (Foo *)F;
    Registers saveArea; //XXX can this make sure saveArea is on the stack

    if (f->input <= 2)
    {

#ifdef DEBUG
        pthread_mutex_lock(&lineLock);
        DEBUG_PRINT("[threadId = %ld, n = %d, line = %d]\n",
            threadId, f->input, line++);
        pthread_mutex_unlock(&lineLock);
#endif

        f->output = 1;
        return;
    }

#ifdef DEBUG
    pthread_mutex_lock(&lineLock);
    int volatile localLine = line++;
    DEBUG_PRINT("[threadId = %ld, n = %d, line = %d]\n",
        threadId, f->input, localLine);
    pthread_mutex_unlock(&lineLock);
#endif

    Foo* a = (Foo *)calloc(1, sizeof(Foo));
    Foo* b = (Foo *)calloc(1, sizeof(Foo));
    a->input = f->input - 1;
    b->input = f->input - 2;

    // longer execution like this?
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 10;
    nanosleep(&ts, NULL);

    // stacklet ===========================
    void* stackPointer;
    getStackPointer(stackPointer);
    Seed* seed = initSeed(&&SecondChildSteal, stackPointer);
    pthread_mutex_lock(&seedStackLock);
    pushSeed(seed);
    int volatile syncCounter = 0; // "volatile" to prevent deadcode elimination,
                                  // and also for synchronization.
    void* volatile firstChildReturnAdr = &&FirstChildDoneNormally;
    void* localStubBase = stubBase; // used more than 12 hours to find this bug...
    saveRegisters();
    pthread_mutex_unlock(&seedStackLock); // uh.... need to lock until here..
    // ====================================

    fib(a);
    restoreRegisters(); // may not need to as we already used "volatile" on
                        // "firstChildReturnAdr"
    pthread_mutex_lock(&seedStackLock);
    goto *firstChildReturnAdr;

FirstChildDoneNormally:
    // stacklet ===========================
    popSeed(seed);
    pthread_mutex_unlock(&seedStackLock);
    // ====================================

    fib(b);
    f->output = a->output + b->output;
    return;

SecondChildSteal: // We cannot make function calls here!
    // stacklet ===========================
    restoreRegisters();
    firstChildReturnAdr = &&FirstChildDone;
    syncCounter = 2;
    stubBase = localStubBase;
    saveRegisters();
    stackletForkStub(&&SecondChildDone, stackPointer, fib, (void *)b);
    // ====================================

FirstChildDone:
    // stacklet ===========================
    restoreRegisters();
    pthread_mutex_unlock(&seedStackLock);
    {
        int localSyncCounter = __sync_sub_and_fetch(&syncCounter, 1);
        if (localSyncCounter != 0)
        {
            saveRegisters();
            suspendStub();
        }
    }
    // ====================================

#ifdef DEBUG
    pthread_mutex_lock(&lineReturnedLock);
    assert(lineReturned[localLine] == 0);
    lineReturned[localLine] = 1;
    pthread_mutex_unlock(&lineReturnedLock);
#endif

    f->output = a->output + b->output;
    return;

SecondChildDone: // We cannot make function calls before we confirm first child
                 // has already returned.
    // stacklet ===========================
    restoreRegisters();
    {
        int localSyncCounter = __sync_sub_and_fetch(&syncCounter, 1);
        if (localSyncCounter != 0)
        {
            saveRegisters();
            suspendStub();
        }
    }
    // ====================================

#ifdef DEBUG
    pthread_mutex_lock(&lineReturnedLock);
    assert(lineReturned[localLine] == 0);
    lineReturned[localLine] = 1;
    pthread_mutex_unlock(&lineReturnedLock);
#endif

    f->output = a->output + b->output;
    return;
}

void *thread(void *arg)
{
  threadId = (long)arg;
  systemStack = systemStackInit();
  suspend();
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
    stackletInit();
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
    if (argc == 3) numthreads = atoi(argv[2]);
    printf("Will run fib(%d) on %d thread(s)\n", n, numthreads);

    int x = startfib(n, numthreads);
    printf("fib(%d) = %d\n", n, x);
    exit(EXIT_SUCCESS);
}
