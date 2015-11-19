// @file myfib.c

#include "myfib.h"
#include "stacklet.h"
#include "seedStack.h"
#include <pthread.h>
#include <assert.h>

#define EMBED_BREAKPOINT bp()
void bp(void) {}

// helper functions
void die(char* str)
{
    fprintf(stderr, "Error: %s\n", str);
    exit(-1);
}

static int suspCtr = 0;
static int suspHere = 2;

void
fib(void* F)
{
    labelhack(FirstChildDoneNormally);
    labelhack(SecondChildSteal);
    labelhack(ChildDone);

    Foo* volatile f = (Foo *)F;
    Registers volatile saveArea; //XXX can this make sure saveArea is on the stack

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
    DEBUG_PRINT("Normal return from n = %d\n", f->input);
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
        suspendStub();
    // ====================================

    f->output = a->output + b->output;
    DEBUG_PRINT("A child return from n = %d\n", f->input);
    return;
}

int 
startfib(int n)
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

    //int x = startfib(n, numthreads);
    int x = startfib(n);

    printf("fib(%d) = %d\n", n, x);
    exit(0);
}
