// @file myfib.c

#include "myfib.h"
#include "stacklet.h"
#include "seedStack.h"
#include <pthread.h>
#include <assert.h>

__thread long threadId;
volatile int line;
pthread_mutex_t lineLock;

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
        pthread_mutex_lock(&lineLock);
        DEBUG_PRINT("[threadId = %ld, n = %d, depth = %d, line = %d]\n",
            threadId, f->input, f->depth, line++);
        pthread_mutex_unlock(&lineLock);
        f->output = 1;
        return;
    }

    pthread_mutex_lock(&lineLock);
    DEBUG_PRINT("[threadId = %ld, n = %d, depth = %d, line = %d]\n",
        threadId, f->input, f->depth, line++);
    pthread_mutex_unlock(&lineLock);

    Foo* a = (Foo *)calloc(1, sizeof(Foo));
    Foo* b = (Foo *)calloc(1, sizeof(Foo));
    a->input = f->input - 1;
    b->input = f->input - 2;

    // stacklet ===========================
    void* stackPointer;
    getStackPointer(stackPointer); //XXX May be we can push ebp, where it points to esp also ??
    Seed* seed = initSeed(&&SecondChildSteal, stackPointer);
    pthread_mutex_lock(&seedStackLock);
    pushSeed(seed);
    pthread_mutex_unlock(&seedStackLock);
    int volatile syncCounter = 0; // "volatile" to prevent deadcode elimination,
                                  // and also for synchronization.
    void* volatile firstChildReturnAdr = &&FirstChildDoneNormally;
    void* localStubBase = stubBase; // used more than 12 hours to find this bug...
    saveRegisters();
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
//    DEBUG_PRINT("Normal return from n = %d\n", f->input);
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
    atomicAdd(syncCounter, -1);
    if (syncCounter != 0)
    {
        saveRegisters();
        suspendStub();
    }
    // ====================================

    f->output = a->output + b->output;
    return;

SecondChildDone: // We cannot make function calls before we confirm first child
                 // has already returned.
    // stacklet ===========================
    restoreRegisters();
    atomicAdd(syncCounter, -1);
    if (syncCounter != 0)
    {
        saveRegisters();
        suspendStub();
    }
    // ====================================

    f->output = a->output + b->output;
    return;
}

void *thread(void *arg)
{
  threadId = (long)arg;
  systemStack = systemStackInit();
  suspend();
}

// Start calculating fib.
//
// Currently we are only doing one thread. We can created multiple pthreads here
// to have the calculation run in multithread mode.
int 
startfib(int n, int numthreads)
{
    labelhack(Start);
    labelhack(End);

    Registers saveArea;

    stackletInit();

    pthread_mutex_init(&lineLock, NULL);

    Foo* a = (Foo *)calloc(1, sizeof(Foo));
    a->input = n;
    a->depth = 1;

    // stacklet ===========================
    void* stackPointer;
    getStackPointer(stackPointer);
    Seed* seed = initSeed(&&Start, stackPointer);
    pushSeed(seed);
    saveRegisters();
    // ====================================

    // set up pthreads
    {
      pthread_t tid[30];
      long i;
      for (i = 0; i < 30; i++)
        pthread_create(&tid[i], NULL, thread, (void*)i);
      for (i = 0; i < 30; i++)
        pthread_join(tid[i], NULL); // this will stall forever
    }

    //assert(0);

Start:
    // stacklet ===========================
    restoreRegisters();
    stackletForkStub(&&End, stackPointer, fib, (void *)a);
    // ====================================
End:
    restoreRegisters();
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
    exit(0);
}
