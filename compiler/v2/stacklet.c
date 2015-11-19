// @file stacklet.c

#include "stacklet.h"
#include "stdio.h"
#include "seedStack.h"
#include "readyQ.h"
#include "debug.h"
#include <stdlib.h>

void* systemStack;
void* volatile stubBase;

void
systemStackInit()
{
    void* systemStackBuffer;
    systemStackBuffer = calloc(1, SYSTEM_STACK_SIZE);
    systemStack = systemStackBuffer + SYSTEM_STACK_SIZE;
}

void
stackletInit()
{
    systemStackInit();
    seedStackInit();
    readyQInit();
}

void
stubRoutine()
{
    DEBUG_PRINT("In stub routine.\n");
    Stub* stackletStub = (Stub *)(stubBase - sizeof(Stub));
    void* buf = stackletStub - STACKLET_SIZE;
 
    DEBUG_PRINT("\tFirst child returns first.\n");
    switchToSysStackAndFreeAndResume(buf, stackletStub->parentSP,
            stackletStub->parentPC, stackletStub->parentStubBase);
}

// Create a new stacklet to run the seed.
void
stackletFork(void* parentPC, void* parentSP, void (*func)(void*), void* arg)
{
    DEBUG_PRINT("Forking a stacklet.\n");
    void* stackletBuf = calloc(1, STACKLET_SIZE);
    DEBUG_PRINT("\tAllocate stackletBuf %p\n", stackletBuf);
    void* newStubBase = stackletBuf + STACKLET_SIZE;
    Stub* stackletStub = (Stub *)(newStubBase - sizeof(Stub));

    stackletStub->parentStubBase = stubBase;
    stackletStub->parentSP = parentSP;
    stackletStub->parentPC = parentPC;
    stackletStub->stubRoutine = stubRoutine;

    stubBase = newStubBase;

    switchAndJmpWithArg(stackletStub, func, arg);
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
        if (seed != NULL)
        {
            void* adr = seed->adr;
            void* sp = seed->sp;
            popSeed(seed);
            switchAndJmp(sp, adr);
        }

        // look at ready Q.  If there is stuff there, grab one and start it
        ReadyThread* ready = readyDummyHead->front;
        if (ready != NULL)
        {
            void* adr = ready->adr;
            void* sp = ready->sp;
            deqReadyQ();
            switchAndJmp(sp, adr);
        }
    }
}

// Yield to another user thread.
void
yield(void)
{
    labelhack(Resume);

    DEBUG_PRINT("Put self in readyQ and suspend\n");
    Registers volatile saveArea;
    void* volatile localStubBase = stubBase;
    DEBUG_PRINT("Store stubBase in localStubBase %p\n", localStubBase);
    saveRegisters();
    void* stackPointer;
    getStackPointer(stackPointer);
    enqReadyQ(&&Resume, stackPointer);

    suspendStub();

Resume:
    restoreRegisters();
    stubBase = localStubBase;
    DEBUG_PRINT("Resumed a ready thread.\n");
}
