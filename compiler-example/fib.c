#include <stdio.h>
#include <stdlib.h>

// asm volatile("", :/*output*/ : /*input*/ : /*clobber*/);

typedef struct {
	void* adr;
	void* sp;
} Continuation;

Continuation seedStack[128];
int seedStackPtr = 0;

#define pushseed(x)	\
    asm volatile("nop; nop; nop; movq %[arg],%[adr]; movq rsp,%[sp]; addq %[sizeOfCont],%[seedSP]" \
	: [seedSP] "=m" (seedStackPtr) \
	: [adr] "m" (seedStack[seedStackPtr].adr), [sp] "m" (seedStack[seedStackPtr].sp), [arg] "r" (&&x), [sizeOfCont] "i" (sizeof(Continuation))); \
    asm goto("" : : : : x)

void popseed()
{
    seedStackPtr--;
}

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
    if (suspCtr++ >= 10) {
	suspCtr = 0;
	Cont2ReadyQ(&&resumeHere);
	saveRegisters();
	suspend();
	return;
    }
 resumeHere:
    return;
}

int 
fib(int n)
{
    int joinCounter;

    testHack();
    if (n < 2) return 1;
    
    pushseed(firstfork);
    int x = fib(n-1);
    popseed();

    int y = fib(n-2);
    /* join */

 joinDone:
    return x+y;

    // get here if child suspends
 firstfork:
    joinCounter = 2;
    adjustChildReturnAdr(&&firstChildDone);
    saveRegisters();

    fork(&fib, &&secondChildDone, n-2);
    return;

 firstChildDone:
    restoreRegisters();
    x = getReturnValue();
    joinCounter--;
    if (joinCounter == 0) goto joinDone;
    saveRegisters();
    suspend();
    return;

 secondChildDone:
    restoreRegisters();
    y = getReturnValue();
    joinCounter--;
    if (joinCounter == 0) goto joinDone;
    saveRegisters();
    suspend();
    return;
}

int 
startfib(int n)
{
    int x = fib(n);
    return x;
}

void
main(int argc, char** argv)
{
    int numthreads = 1;
    if (argc < 1) die("Need at least one argument to run, n.  If 2, then second is # of threads");
    int n = atoi(argv[0]);
    if (argc == 2) numthreads = atoi(argv[1]);
    printf("Will run fib(%d) on %d thread(s)\n", n, numthreads);

    int x = startfib(n);

    printf("fib(%d) = %d\n", n, x);
    exit(0);
}
