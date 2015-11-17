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
stubRoutine()
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

// The function to to lazy fork recursively.
//
// Inorder to minimize the overhead of a potential parallel sequential call, we
// do as little work as possible when inserting seeds to the seed stack.
//
// The following example which we use to explain how the code works assumes we
// are running on a single core machine with a single thread.
//
// The first lfork will always be a sequential call. But before we call it:
// 1) We put the sp of the current function frame (assume the compiler uses sp
//    to reference local variables stored on the stack) and the address of the
//    label from where if any thread wants to steal the second lfork should
//    resume, on the seed stack (let's call it "SecondChildSteal").
// 2) We store all the registers on the stack in a reserved area.
//
// For every several seeds we push to the seed stack, we call "yield", where we:
// 1) Save the stubBase and registers on the stack.
// 2) Put the current user thread to the readyQ. The information we need to
//    stored in the readyQ includes the sp of current frame and the address of
//    the label to resumes (in this case we choose "Resume" as the label name)
// 3) Change the stack pointer to "system stack".
// 4) Call "suspend".
//
// In"suspend", we:
// 1) Check if there is any seed in the seed stack. If so, then we will steal
//    the work this seed represents by:
//    (1) Change the stack pointer to the sp we put in the seedstack before.
//    (2) Change the pc to the pc we put in the seedstack before, where we -->
//    (3) Restore the registers which have been stored on the stack. This step
//        is necessary because some of the local variables may be stored in
//        the registers.
//    (4) Change the return address of the first call to "FirstChildDone".
//    (5) Change the synchronization counter to be 2.
//    (5) Store the registers on the stack in the same reversed area.
//    (6) Change the stack pointer to a "system stack".
//    (7) Call "stackletFork" with the return address "SecondChildDone", the
//        stack pointer of the current function as parameters, the function to
//        execute and the paremeter for the function.
// 2) Check if there is any thread in the readyQ. If so, then we will resume
//    the work of this user thread by:
//    (1) Change the stack pointer to the sp stored in the readyQ.
//    (2) Change the pc to the resume label, where we -->
//    (3) Restore the registers and stubBase.
//    (4) Dequeue the node in the readyQ.
//    (5) Return. By return, we resumes whatever is left before.
// 3) We do the above in a while(1) loop
//
// In "stackletFork", we:
// 1) Malloc a "stacklet" and set up the stacklet as following (from high
//    memory address to low memory address):
//    +----------------+
//    | parentStubBase | <-- stubBase points to the top of the stacket
//    +----------------+
//    | parentSP       | <-- the sp of the parent function frame
//    +----------------+
//    | parentPC       | <-- where to resume, in this case, "SecondChildDone"
//    +----------------+
//    | stubRoutine    | <-- deallocate stackframe and goes back to parent
//    +----------------+ <-- RSP
// 2) Then we change the stack pointer to "RSP" as indicated above, move
//    parameter to rdi and the jmp to the function to "fork".
//
// Now let's say we finishes executing the second call, we will call "ret" in
// the last line of the function. We goes into "stubRoutine" naturally, where:
// 1) Change the stack pointer to "system stack".
// 2) Store all the information on the current stub in local variables.
// 3) Free the stacklet.
// 4) Restore the parent stub base, parent sp and then goto parent pc.
//
// Now we are back in the parent frame executing from "SecondChildDone". What we
// do here is:
// 1) Restore all the registers (of course).
// 2) Subtract 1 from the synchronization counter.
// 3) Check if the synchronization counter is zero. If so, this means the first
//    call of fib has already been returned and we can continue the job on this
//    stacklet by return from this function. If not, we:
//    (1) Change the stack pointer to "system stack"
//    (2) Call "suspend"
//
// You know the rest of the story...
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
