// @file myfib.c
#include "myfib.h"
#include "seedStack.h"
#include "readyQ.h"
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

void* systemStack;

void
systemStackInit()
{
    void* systemStackBuffer;
    systemStackBuffer = calloc(1, SYSTEM_STACK_SIZE);
    systemStack = systemStackBuffer + SYSTEM_STACK_SIZE;
}

typedef struct {
    void* stubRoutine;
    Seed* seed;
    void* stackletBuf;
} Stub;

void* volatile stubBase;

void
stubRoutineFunctionWrapper()
{
    DEBUG_PRINT("In stub routine.\n");
    Stub* stackletStub = (Stub *)(stubBase - sizeof(Stub));
    Seed* seed = stackletStub->seed;
    void* buf = stackletStub->stackletBuf;
 
    if (--seed->joinCounter == 0)
    {
        DEBUG_PRINT("\tFirst child returns first.\n");
        switchToSysStackAndFreeAndResume(buf, seed->sp, seed->adr,
                                         seed->parentStubBase);
    }

    DEBUG_PRINT("\tSecond child returns first.\n");
    switchToSysStackAndFree(buf);
    // sp -> sysStack
    suspend();
}

void
stubRoutine()
{
    stubRoutineFunctionWrapper();
}

// Create a new stacklet to run the seed.
void
stackletFork(Seed* seed)
{
    DEBUG_PRINT("Forking a stacklet.\n");
    seed->joinCounter = 2;
    void* stackletBuf = calloc(1, STACKLET_SIZE);
    DEBUG_PRINT("\tAllocate stackletBuf %p\n", stackletBuf);
    stubBase = stackletBuf + STACKLET_SIZE;
    Stub* stackletStub = (Stub *)(stubBase - sizeof(Stub));

    stackletStub->stackletBuf = stackletBuf;
    stackletStub->seed = seed;
    stackletStub->stubRoutine = stubRoutine;

    setArgument(seed->argv);
    void* routine = seed->routine; 
    restoreStackPointer(stackletStub);
    goto *routine; //XXX rsp is already changed
}

// Context switch to another user thread in readyQ (explicit seed) or in seed
// stack (implicity seed).
//
// The current user thread will be lost if it is not put into the readyQ before
// calling this function. This funciton is executed in a separate system stack.
void
suspend()
{
    for (;;)
    {
        // look at seedStack.  If there is stuff there, grab it
        Seed* seed = seedDummyHead->next;
        while (seed != NULL)
        {
            if (seed->activated == 0)
            {
                seed->activated = 1;
                stackletFork(seed);
            }
            seed = seed->next;
        }

        // look at ready Q.  If there is stuff there, grab one and start it
        ReadyThread* ready = readyDummyHead->front;
        if (ready != NULL)
        {
            //DEBUG_PRINT("Are to resume a ready thread.\n");
            restoreStackPointer(ready->sp);
            goto *ready->adr;
        }
    }
}

#define labelhack(x) \
    asm goto("" : : : : x)

void
yield(void)
{
    labelhack(Resume);

    DEBUG_PRINT("Put self in readyQ and suspend\n");
    volatile Registers saveArea;
    void* volatile localStubBase = stubBase;
    DEBUG_PRINT("Store stubBase in localStubBase %p\n", localStubBase);
    saveRegisters();
    void* stackPointer;
    getStackPointer(stackPointer);
    enqReadyQ(&&Resume, stackPointer);

    switchToSysStack();
    // rsp -> sysStack
	suspend(); // suspend will never return

Resume:
    restoreRegisters();
    stubBase = localStubBase;
    deqReadyQ();
    DEBUG_PRINT("Resumed a ready thread.\n");
}

void
fib(void* F)
{
    labelhack(FirstChildDone);
    labelhack(SecondChildDone);

    Foo* volatile f = (Foo *)F;
    bp();
    Registers volatile saveArea; //XXX can this make sure saveArea is on the stack

    if (f->input <= 2)
    {
        DEBUG_PRINT("[n = %d, depth = %d]\n", f->input, f->depth);
        f->output = 1;
        return;
    }

    // testHack
    if (suspCtr++ >= suspHere) {
        suspCtr = 0;
        yield();
    }
    DEBUG_PRINT("[n = %d, depth = %d]\n", f->input, f->depth);
    //testHack();

    Foo* volatile a = (Foo *)calloc(1, sizeof(Foo));
    Foo* volatile b = (Foo *)calloc(1, sizeof(Foo));
    a->input = f->input - 1;
    a->depth = f->depth + 1;
    b->input = f->input - 2;
    b->depth = f->depth + 1;

    void* stackPointer;
    getStackPointer(stackPointer);
    Seed* volatile seed = initSeed(&&SecondChildDone, stackPointer, fib, (void *)b, stubBase);
    pushSeed(seed);

    saveRegisters();
    fib(a);

    if (seed->activated)
    {
        if (--seed->joinCounter) {
            DEBUG_PRINT("First child returns first.\n");
            switchToSysStack();
            // rsp -> sysStack
            suspend();
        } else {
            goto FirstChildDone;
        }
    }
    
    popSeed(seed);
    fib(b);
    f->output = a->output + b->output;
    DEBUG_PRINT("Normal return from n = %d\n", f->input);
    return;

FirstChildDone:
    popSeed(seed);
    f->output = a->output + b->output;
    DEBUG_PRINT("First child return from n = %d\n", f->input);
    return;

SecondChildDone:
    restoreRegisters(); // foo, a, b may be stored in any of the registers
    popSeed(seed);
    f->output = a->output + b->output;
    DEBUG_PRINT("Second child return from n = %d\n", f->input);
    return;
}

int 
startfib(int n)
{
    systemStackInit();
    seedStackInit();
    readyQInit();

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
    if (argc < 2) die("Need at least one argument to run, n.  If 2, then second is # of threads");
    int n = atoi(argv[1]);
    if (argc == 3) numthreads = atoi(argv[2]);
    printf("Will run fib(%d) on %d thread(s)\n", n, numthreads);

    //int x = startfib(n, numthreads);
    int x = startfib(n);

    printf("fib(%d) = %d\n", n, x);
    exit(0);
}
