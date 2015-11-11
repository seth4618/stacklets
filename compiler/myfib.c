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

#define switchToSysStack() do { \
    asm volatile("movq %[sysStack],%%rsp \n"\
                 :\
                 : [sysStack] "m" (systemStack));} while (0)

#define switchToSysStackAndFree(buf) do { \
    asm volatile("movq %[Abuf],%%rdi \n"\
                 "movq %[AsystemStack],%%rsp \n"\
                 "call _free \n"\
                 :\
                 : [Abuf] "r" (buf),\
                   [AsystemStack] "m" (systemStack));} while (0)

#define switchToSysStackAndFreeAndResume(buf,sp,adr) do {\
    asm volatile("movq %[Abuf],%%rdi \n"\
                 "movq %[Asp],%%rax \n"\
                 "movq %[Aadr],%%rbx \n"\
                 "movq %[AsystemStack],%%rsp \n"\
                 "call _free \n"\
                 "movq %%rax,%%rsp \n"\
                 "jmp *%%rbx \n"\
                 :\
                 : [Abuf] "r" (buf),\
                   [Asp] "r" (sp),\
                   [Aadr] "r" (adr),\
                   [AsystemStack] "m" (systemStack));} while (0)

//#define dealloc(x) do { \
//    asm volatile("movq %[sysStack],%%rsp \n"\
                 

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
    DEBUG_PRINT("In stub routine.\n");
    Stub* stackletStub = (Stub *)(stubBase - sizeof(Stub));
    Seed* seed = stackletStub->seed;
    void* buf = stackletStub->stackletBuf;
 
    if (--seed->joinCounter == 0)
    {
        DEBUG_PRINT("\tFirst child returns first.\n");
        switchToSysStackAndFreeAndResume(buf, seed->sp, seed->adr);
        // sp -> sysStack
        //restoreStackPointer(seed->sp);
        //goto *seed->adr;
    }

    DEBUG_PRINT("\tSecond child returns first.\n");
    switchToSysStackAndFree(buf);
    // sp -> sysStack
    suspend();
}

// Create a new stacklet to run the seed.
void
stackletFork(Seed* seed)
{
    DEBUG_PRINT("Forking a stacklet.\n");
    seed->joinCounter = 2;
    void* stackletBuf = calloc(1, STACKLET_SIZE);
    stubBase = stackletBuf + STACKLET_SIZE;
    Stub* stackletStub = (Stub *)(stubBase - sizeof(Stub));

    stackletStub->stackletBuf = stackletBuf;
    stackletStub->seed = seed;
    stackletStub->stubRoutine = stubRoutine;

    setArgument(seed->argv);
    restoreStackPointer(stackletStub);
    void* routine = seed->routine; //XXX rsp is already changed
    goto *routine;
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

#define labelhack(x) \
    asm goto("" : : : : x)

void
yield(void)
{
    DEBUG_PRINT("Put self in readyQ and suspend\n");
    Registers saveArea;
    void* localStubBase = stubBase;
    saveRegisters();
    void* stackPointer;
    getStackPointer(stackPointer);
    labelhack(Resume);
    enqReadyQ(&&Resume, stackPointer);

    switchToSysStack();
    // rsp -> sysStack
	suspend(); // suspend will never return

Resume:
    restoreRegisters();
    deqReadyQ();
    stubBase = localStubBase;
    DEBUG_PRINT("Resumed a ready thread.\n");
}

void
fib(void* F)
{
    Foo* f = (Foo *)F;
    bp();
    volatile Registers saveArea;

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

    Foo* a = (Foo *)calloc(1, sizeof(Foo));
    Foo* b = (Foo *)calloc(1, sizeof(Foo));
    a->input = f->input - 1;
    a->depth = f->depth + 1;
    b->input = f->input - 2;
    b->depth = f->depth + 1;

    void* stackPointer;
    getStackPointer(stackPointer);
    Seed* seed = initSeed(&&SecondChildDone, stackPointer, fib, (void *)b);
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
    popSeed(seed);
    restoreRegisters(); // foo, a, b may be stored in any of the registers
    f->output = a->output + b->output;
    DEBUG_PRINT("Second child return from n = %d\n", f->input);
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
