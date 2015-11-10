// @file myfib.c
#include "myfib.h"
#include "seedStack.h"
#include "readyQ.h"
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#define EMBED_BREAKPOINT bp()
void bp(void) {}

#ifdef DEBUG
#define DEBUG_PRINT(fmt, args...)    fprintf(stderr, fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...)    /* Don't do anything in release builds */
#endif

// helper functions
void die(char* str)
{
    fprintf(stderr, "Error: %s\n", str);
    exit(-1);
}

static int suspCtr = 0;
static int suspHere = 1;

void* systemStack;
#define SYSTEM_STACK_SIZE   8192
#define STACKLET_SIZE       8192

void
systemStackInit()
{
    void* systemStackBuffer;
    systemStackBuffer = calloc(1, SYSTEM_STACK_SIZE);
    systemStack = systemStackBuffer + SYSTEM_STACK_SIZE;
}

#define suspend() do { \
    asm volatile("movq %[sysStack],%%rsp \n"\
                 "call _realSuspend \n"\
                 :\
                 : [sysStack] "m" (systemStack));} while (0)

#define setArgument(x) do { \
    asm volatile("movq %[argv],%%rdi \n"\
                 :\
                 : [argv] "m" (x));} while (0)

typedef struct {
    void* stubRoutine;
    Seed* seed;
    void* stackletBuf;
} Stub;

void* stubBase;

void
stubRoutine()
{
    Stub* stackletStub = (Stub *)(stubBase - sizeof(Stub));
    Seed* seed = stackletStub->seed;
    void* buf = stackletStub->stackletBuf;
 
    if (--seed->joinCounter == 0)
    {
        free(buf);
        restoreStackPointer(seed->sp);
        goto *seed->adr;
    }

    free(buf);
    suspend();
}

// Create a new stacklet to run the seed.
void
stackletFork(Seed* seed)
{
    seed->joinCounter = 2;
    void* stackletBuf = calloc(1, STACKLET_SIZE);
    stubBase = stackletBuf + STACKLET_SIZE;
    Stub* stackletStub = (Stub *)(stubBase - sizeof(Stub));

    stackletStub->stackletBuf = stackletBuf;
    stackletStub->seed = seed;
    stackletStub->stubRoutine = stubRoutine;

    setArgument(seed->argv);
    void* routine = seed->routine;
    goto *routine;
}

// Context switch to another user thread in readyQ (explicit seed) or in seed
// stack (implicity seed).
//
// The current user thread will be lost if it is not put into the readyQ before
// calling this function. This funciton is executed in a separate system stack.
__attribute__ ((used))
void
realSuspend()
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
            DEBUG_PRINT("find a ready thread\n");
            restoreStackPointer(ready->sp);
            goto *ready->adr;
        }
    }
}

// will (semi) randomly put this thread on readyQ and suspend it.
//void
//testHack(void)
//{
//    if (suspCtr++ >= suspHere) {
//	    suspCtr = 0;
//        saveRegisters();
//        void* stackPointer;
//        getStackPointer(stackPointer);
//        enqReadyQ(stackPointer);
//	    suspend(Yield); // suspend will never return
//    }
//}

void
yield(void)
{
    Registers saveArea;
    saveRegisters();
    void* stackPointer;
    getStackPointer(stackPointer);
    enqReadyQ(&&Resume, stackPointer);
	suspend(); // suspend will never return
Resume:
    deqReadyQ();
    restoreRegisters();
}

void
fib(void* F)
{
    Foo* f = (Foo *)F;
    DEBUG_PRINT("n = %d\n", f->input);
    bp();
    volatile Registers saveArea;

    if (f->input <= 2)
    {
        f->output = 1;
        return;
    }

    // testHack
    if (suspCtr++ >= suspHere) {
        suspCtr = 0;
        yield();
    }
    //testHack();

    Foo* a = (Foo *)calloc(1, sizeof(Foo));
    Foo* b = (Foo *)calloc(1, sizeof(Foo));
    a->input = f->input - 1;
    b->input = f->input - 2;

    void* stackPointer;
    getStackPointer(stackPointer);
    Seed* seed = initSeed(&&SecondChildDone, stackPointer, fib, (void *)b);
    pushSeed(seed);

    saveRegisters();
    fib(a);

    if (seed->activated && --seed->joinCounter)
    {
        suspend();
    }
    
    popSeed(seed);
    fib(b);
    f->output = a->output + b->output;
    return;

SecondChildDone:
    popSeed(seed);
    restoreRegisters(); // foo, a, b may be stored in any of the registers
    f->output = a->output + b->output;
}

int 
startfib(int n)
{
    systemStackInit();
    seedStackInit();
    readyQInit();

    Foo* a = (Foo *)calloc(1, sizeof(Foo));
    a->input = n;
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
