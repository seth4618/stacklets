// @file myfib.c

#include "myfib.h"
#include "stacklet.h"
#include "seedStack.h"
#include <pthread.h>
#include <assert.h>

static int suspCtr = 0;
static int suspHere = 2;

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
        DEBUG_PRINT("[n = %d, depth = %d]\n", f->input, f->depth);
        f->output = 1;
        return;
    }

    // testHack ===========================
    if (suspCtr++ >= suspHere) {
        suspCtr = 0;
        yield();
    }
    // ====================================
    DEBUG_PRINT("[n = %d, depth = %d]\n", f->input, f->depth);

    Foo* a = (Foo *)calloc(1, sizeof(Foo));
    Foo* b = (Foo *)calloc(1, sizeof(Foo));
    a->input = f->input - 1;
    a->depth = f->depth + 1;
    b->input = f->input - 2;
    b->depth = f->depth + 1;

    // stacklet ===========================
    void* stackPointer;
    getStackPointer(stackPointer); //XXX May be we can push ebp, where it points to esp also ??
    Seed* seed = initSeed(&&SecondChildSteal, stackPointer);
    pushSeed(seed);
    int volatile syncCounter = 0; // "volatile" to prevent deadcode elimination
    void* volatile firstChildReturnAdr = &&FirstChildDoneNormally;
    void* localStubBase = stubBase; // used more than 12 hours to find this bug...
    saveRegisters();
    // ====================================

    fib(a);
    restoreRegisters(); // may not need to as we already used "volatile" on
                        // "firstChildReturnAdr"
    goto *firstChildReturnAdr;

FirstChildDoneNormally:
    // stacklet ===========================
    popSeed(seed);
    // ====================================

    fib(b);
    f->output = a->output + b->output;
    DEBUG_PRINT("Normal return from n = %d\n", f->input);
    return;

SecondChildSteal:
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
    if (--syncCounter != 0)
    {
        DEBUG_PRINT("syncCounter is at %p.\n", &syncCounter);
        DEBUG_PRINT("First child finishes first.\n");
        saveRegisters();
        suspendStub();
    }
    // ====================================

    DEBUG_PRINT("syncCounter is at %p.\n", &syncCounter);
    DEBUG_PRINT("First child finishes last.\n");
    f->output = a->output + b->output;
    DEBUG_PRINT("A child return from n = %d\n", f->input);
    return;

SecondChildDone:
    // stacklet ===========================
    restoreRegisters();
    if (--syncCounter != 0)
    {
        // Cannot make any function calls here when second child finishes first.
        // This is different from the case the first child finishes first.
        saveRegisters();
        suspendStub();
    }
    // ====================================

    DEBUG_PRINT("syncCounter is at %p.\n", &syncCounter);
    DEBUG_PRINT("Second child finishes last.\n");
    f->output = a->output + b->output;
    DEBUG_PRINT("A child return from n = %d\n", f->input);
    return;
}

// Start calculating fib.
//
// Currently we are only doing one thread. We can created multiple pthreads here
// to have the calculation run in multithread mode.
int 
startfib(int n, int numthreads)
{
    stackletInit();

    Foo* a = (Foo *)calloc(1, sizeof(Foo));
    a->input = n;
    a->depth = 1;
    fib(a);
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
