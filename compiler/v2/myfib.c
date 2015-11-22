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
    labelhack(ChildDone);

    // The compiler will assign 0 to these local variables after label if we do
    // not specify volatile attribute here.
    Foo* volatile f = (Foo *)F;
    Registers volatile saveArea; //XXX can this make sure saveArea is on the stack

    if (f->input <= 2)
    {
//        DEBUG_PRINT("[n = %d, depth = %d]\n", f->input, f->depth);
        f->output = 1;
        return;
    }

    // testHack ===========================
    if (suspCtr++ >= suspHere) {
        suspCtr = 0;
        yield();
    }
    // ====================================
//    DEBUG_PRINT("[n = %d, depth = %d]\n", f->input, f->depth);

    Foo* volatile a = (Foo *)calloc(1, sizeof(Foo));
    Foo* volatile b = (Foo *)calloc(1, sizeof(Foo));
    a->input = f->input - 1;
    a->depth = f->depth + 1;
    b->input = f->input - 2;
    b->depth = f->depth + 1;

    // stacklet ===========================
    void* volatile stackPointer;
    getStackPointer(stackPointer); //XXX May be we can push ebp, where it points to esp also ??
    Seed* volatile seed = initSeed(&&SecondChildSteal, stackPointer);
    pushSeed(seed);
    int volatile syncCounter = 0;
    void* volatile firstChildReturnAdr = &&FirstChildDoneNormally;
    saveRegisters();
    // ====================================

    fib(a);
    goto *firstChildReturnAdr;

FirstChildDoneNormally:
    // stacklet ===========================
    popSeed(seed);
    // ====================================

    fib(b);
    f->output = a->output + b->output;
//    DEBUG_PRINT("Normal return from n = %d\n", f->input);
    return;

SecondChildSteal:
    // stacklet ===========================
    restoreRegisters();
    firstChildReturnAdr = &&ChildDone;
    syncCounter = 2;
    saveRegisters();
    stackletForkStub(&&ChildDone, stackPointer, fib, (void *)b);
    // ====================================

ChildDone:
    // stacklet ===========================
    restoreRegisters();
    if (--syncCounter != 0)
    {
        saveRegisters();
        suspendStub();
    }
    // ====================================

    f->output = a->output + b->output;
//    DEBUG_PRINT("A child return from n = %d\n", f->input);
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