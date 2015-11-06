#include <stdio.h>
#include <stdlib.h>

// asm volatile("", :/*output*/ : /*input*/ : /*clobber*/);

typedef int allregs[16] Registers;

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

#define getIntReturnValue(x)	\
    asm volatile("movq rax,%[output]" : %[output] "=r" (x))

#define restoreRegisters() \
    asm volatile("pop rcx; pop rdx; pop rbx; pop rbp; pop rsi; pop rdi; pop r8; pop r9; pop r10; pop r11; pop r12; pop r13; pop r14; pop r15;");

#define saveRegisters() \
    asm volatile("push r15; push r14; push r13; push r12; push r11; push r10; push r9; push r8; push rdi; push rsi; push rbp; push rbx; push rdx; push rcx;");

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

void
suspend(void)
{
    // look at ready Q.  If there is stuff there, grab one and start it
    // look at seedStack.  If there is stuff there, grab it (and detach current child)
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
    getIntReturnValue(x);
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
