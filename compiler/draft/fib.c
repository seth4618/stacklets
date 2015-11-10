#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "fib.h"

// asm volatile("", :/*output*/ : /*input*/ : /*clobber*/);

typedef uint64_t Registers[16];

// forward declarations
void suspend();
void forkAndReturn(void* retAdr, uint64_t fp, int (*func)(int), int n);

// Stores information of a implicit seed.
typedef struct {
	void* adr;
	void* fp;
} Continuation;

Continuation seedStack[128];
int seedStackPtr = 0;

Continuation* popseed()
{
    if (seedStackPtr == 0) return NULL;
    return seedStack + (seedStackPtr--);
}

// Stores information of a explicit seed.
Continuation readyQ[128];
int readyQFront = 0;
int readyQBack = 0;
void dequeue()
{
    readyQFront++;
    readyQFront %= sizeof(128);
}

//#define Cont2ReadyQ(x) do { \
//    asm volatile("

void die(char* str)
{
    fprintf(stderr, "Error: %s\n", str);
    exit(-1);
}

static int suspCtr = 0;
static int suspHere = 10;

// will (semi) randomly put this thread on readyQ and suspend it.
void
testHack(void)
{
    Registers saveArea;
    if (suspCtr++ >= 10) {
	    suspCtr = 0;
        saveRegisters();
	    Cont2ReadyQ(&&resumeHere);

        restoreSystemStackPointer();
	    suspend(); // suspend will never return
    }
 resumeHere:
    restoreRegisters();
    return;
}

// Context switch to another user thread in readyQ (explicit seed) or in seed
// stack (implicity seed).
//
// The current user thread will be lost if it is not put into the readyQ before
// calling this function. This funciton is executed in a separate system stack.
void
suspend(void)
{
    Continuation* seed;
    // look at seedStack.  If there is stuff there, grab it
    if ((seed = popseed()) != NULL)
    {
        restoreFramePointer(seed->sp);
        goto *seed->adr; // FIXME
    }

    // look at ready Q.  If there is stuff there, grab one and start it
    if (readyQFront != readyQBack)
    {
        int tmp = readyQFront;
        dequeue();
        restoreFramePointer(readyQ[tmp].sp);
        goto *readyQ[tmp].adr;
    }
}

// the stacklet stub struct
#define STACKLET_SIZE 8192

typedef struct {
    void (*stubRoutine)();
    uint64_t top;
    uint64_t fp;
    void* retAdr;
} Stub;

void stubRoutine()
{
}

// Allocate a new stacklet and put the stacklet address in the readyQ.
//
// In the stub structure of each stacklet, we need to store in order from high
// address to low adress:
// real return address - This is also called the inlet.
// frame pointer of the parent - As the next instruct (in our fib example) is
//                               return, rsp will also be set properly (by
//                               instruction movq %rbp, %rsp; popq %rbp; ret;)
// top pointer of the stacklet
// address to enter stub routine
//
// The function is executed immediately in the newly allocated stacklet.
void
fork(void* retAdr, uint64_t fp, int (*func)(int), int n)
{
    forkAndReturn(retAdr, fp, func, n);

    // execute stealed work
}

// Fork but return instead of executing immediately.
//
// This version of fork is only called once.
void
forkAndReturn(void* retAdr, uint64_t fp, int (*func)(int), int n)
{
    // allocate stub in heap
    char* stacklet = (char *)malloc(STACKLET_SIZE);
    Stub* stub = (Stub *)(stacklet + STACKLET_SIZE - sizeof(Stub));

    // setup stub
    stub->stubRoutine = stubRoutine;
    stub->top = (uint64_t)stub;
    stub->fp = fp;
    stub->retAdr = retAdr;

    // put the stacklet address in the readyQ
}

// XXX: we assume rax is not used to store local varaibles.
int 
fib(int n)
{
    int joinCounter;
    volatile Registers saveArea;    // "volatile" to ensure it's on the stack
    void* pcrc = &&normalReturn;    // The address the first fib will return
                                    // (jmp) to.
    if (n <= 2) return 1;

    //saveRegisters();

    testHack();

    pushseed(firstfork);
    int x = fib(n-1);
    goto *pcrc;

 normalReturn:
    popseed();

    int y = fib(n-2);
    /* join */

 joinDone:
    return x+y;

    // get here if child suspends
 firstfork:
    //restoreRegisters();         // mainly for restoring rsp
    joinCounter = 2;            // set sync counter
    pcrc = &&firstChildDone;    // modify child return address
    saveRegisters();            // save modification of local variables

    // transfer control to the second fib
    restoreSystemStackPointer();
    uint64_t rbp;
    getFramePointer(rbp);
    fork(&fib, rbp, &&secondChildDone, n-2);

 firstChildDone:
    restoreRegisters();
    getIntReturnValue(x);
    joinCounter--;
    if (joinCounter == 0) goto joinDone;
    saveRegisters();

    restoreSystemStackPointer();
    suspend(); // suspend will never return

 secondChildDone:
    restoreRegisters();
    getIntReturnValue(y);
    joinCounter--;
    if (joinCounter == 0) goto joinDone;
    saveRegisters();

    restoreSystemStackPointer();
    suspend();
}

// Worker thread.
//
// The worker thread will check if there is any work in seedStack in a polling
// manner. If so, the worker thread will "steal" the work to run in a new
// stacklet.
//void *thread(void *vargp)
//{
//    fprintf(stderr, "thread id = %x\n", (unsigned int)pthread_self());
//
//    for(;;) suspend();
//}

int 
startfib(int n)
{
    int x = fib(n);
    return x;
}

// Calculate fib.
//
// We create number of threads the user asked for to calculate fib. The function
// each pthread is running is an infinite loop. The program terminates only when
// one of the pthread reach "Done", which means the result is available.
//int
//startfib(int n, int numthreads)
//{
//    int rc;
//
//    uint64_t rbp;
//    getFramePointer(rbp);
//    forkAndReturn(&fib, rbp, &&Done, n);
//
//    // create worker pthreads
//    pthread_t tid[numthreads];
//    long i;
//    for (i = 0; i < numthreads; i++)
//        pthread_create(&tid[i], NULL, thread, (void *)i);
//    for (i = 0; i < numthreads; i++)
//        pthread_join(tid[i], NULL);
//
//  Done:
//    getIntReturnValue(rc);
//    return rc;
//}

int
main(int argc, char** argv)
{
    int numthreads = 1;
    if (argc < 1) die("Need at least one argument to run, n.  If 2, then second is # of threads");
    int n = atoi(argv[0]);
    if (argc == 2) numthreads = atoi(argv[1]);
    printf("Will run fib(%d) on %d thread(s)\n", n, numthreads);

    //int x = startfib(n, numthreads);
    int x = startfib(n);

    printf("fib(%d) = %d\n", n, x);
    exit(0);
}
