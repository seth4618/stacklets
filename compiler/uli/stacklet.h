// @file stacklet.h
#pragma once

#include <stdint.h>
#include <pthread.h>
#include "spinlock.h"
#ifdef ULI
#include "msgs.h"
#endif

//#define MACOS

extern __thread int threadId;
extern int numberOfThreads;

typedef uint64_t Registers[16];

typedef struct stubstruct Stub;

struct stubstruct {
    void* stubRoutine;
    void* parentPC;
    void* parentSP;
    int parentProc;		/* processor that parent is running on */
    int pad3;
    Stub* next;
    Stub* prev;
    int allocatorThread;	/* thread that is creating the stacklet */
    int seedThread;		/* thread that created the seed */
    void* pad2;
};

extern __thread void* systemStack;
extern SpinLockType readyQLock;

#define SYSTEM_STACK_SIZE   	(8192*2)
#define STACKLET_SIZE       	81920
#define STACKLET_BUF_ZEROS	7
#define STACKLET_ALIGNMENT	(1<<STACKLET_BUF_ZEROS)
#define RE_ALIGNMENT_FUDGE	((1<<(STACKLET_BUF_ZEROS-1)))
void stackletInit(int numThreads);
void* systemStackInit();
void stubRoutine();


#ifdef ULI
void stackletFork(void* parentPC, void* parentSP, void (*func)(void*), void* arg, BasicMessage* msg);
#else
void stackletFork(void* parentPC, void* parentSP, void (*func)(void*), void* arg, int tid);
#endif

void suspend();
void yield(void);

#define getStackletStub(stackletStub) do {\
asm volatile("movq %%rsp, %[AstackletStub] \n"\
             : [AstackletStub] "=r" (stackletStub) \
             :);} while (0)

#define labelhack(x) \
    asm goto("" : : : : x)

#ifdef MACOS
#define suspendStub() do {\
asm volatile("movq %[sysStack], %%rsp \n"\
             "call _suspend \n"\
             :\
             : [sysStack] "m" (systemStack));} while (0)
#else
#define suspendStub() do {\
asm volatile("movq %[sysStack], %%rsp \n"\
             "call suspend \n"\
             :\
             : [sysStack] "m" (systemStack));} while (0)
#endif

#ifdef MACOS
#define stackletForkStub(parentPC, parentSP, func, arg, tid) do {\
asm volatile("movq %[AparentPC], %%rdi \n"\
             "movq %[AparentSP], %%rsi \n"\
             "movq %[Afunc], %%rdx \n"\
             "movq %[Aarg], %%rcx \n"\
             "movl %[TIDarg], %%r8d \n"\
             "movq %[sysStack], %%rsp \n"\
             "call _stackletFork \n"\
             :\
             : [AparentPC] "r" (parentPC),\
               [AparentSP] "r" (parentSP),\
               [Afunc] "r" (func),\
               [Aarg] "r" (arg),\
               [TIDarg] "r" (tid),\
               [sysStack] "m" (systemStack)\
             : "rdi", "rsi", "rdx", "rcx", "r8");} while (0)
#else
#define stackletForkStub(parentPC, parentSP, func, arg, tid, msg) do {	\
asm volatile("\t#calling stackletFork\n"\
	     "movq %[AparentPC], %%rdi \n"\
             "movq %[AparentSP], %%rsi \n"\
             "movq %[Afunc], %%rdx \n"\
             "movq %[Aarg], %%rcx \n"\
             "movq %[aMsg], %%r8 \n"\
             "movq %[sysStack], %%rsp \n"\
             "call stackletFork \n"\
             :\
             : [AparentPC] "r" (parentPC),\
               [AparentSP] "r" (parentSP),\
               [Afunc] "r" (func),\
               [Aarg] "r" (arg),\
               [aMsg] "r" (msg),\
               [sysStack] "m" (systemStack)\
             : "rdi", "rsi", "rdx", "rcx", "r8");} while (0)
#endif

void firstFork(void (*func)(void*), void* arg);

#define switchAndJmpWithArg(sp, adr, arg, msg) do {	\
asm volatile("movq %[Aarg], %%rdi \n"\
             "movq %[Amsg], %%rsi \n"\
             "movq %[Asp], %%rsp \n"\
             "jmp *%[Aadr] \n"\
             :\
             : [Asp] "r" (sp),\
               [Aadr] "r" (adr),\
               [Amsg] "r" (msg),\
               [Aarg] "r" (arg)\
             : "rdi", "rsi");} while (0)

// used to restart a stacklet which is on the ready queue
#define localSwitchAndJmp(sp,adr) do {		\
asm volatile("movq %[Asp],%%rsp \n"\
             "jmp *%[Aadr] \n"\
             :\
             : [Asp] "r" (sp),\
               [Aadr] "r" (adr)\
             );} while(0)


// used to start running a child steal routine.  We are stealing from thread tid.
// the steal routine will assume %rax has tid from whom we are stealing
// See recoverParentTID
//#define switchAndJmp(sp,adr,tid) do {		\
//asm volatile("movq %[Asp],%%rsp \n"\
//             "movl %[Itid], %%eax \n"\
//             "jmp *%[Aadr] \n"\
//             :\
//             : [Asp] "r" (sp),\
//	       [Itid] "r" (tid),\
//               [Aadr] "r" (adr));} while(0)
#define switchAndJmp(sp,adr,tid,msg) do {		\
asm volatile("movq %[Asp],%%rsp \n"\
             "movq %[Amsg], %%rdi \n"\
             "jmp *%[Aadr] \n"\
             :\
             : [Asp] "r" (sp),\
	       [Amsg] "r" (msg),\
               [Aadr] "r" (adr)\
             : "rdi");} while(0)

// right after restoring registers this is needed
// see switchAndJmp
//#define recoverParentTID(tid) \
//    asm volatile("movl %%eax,%[Itid]"\
//		 : [Itid] "=r" (tid)	\
//                 :\
//		 : "eax")

#if 1
#define switchToSysStackAndFinishStub(buf,ss) do {\
asm volatile("#switch from stacket sp to system sp and finish up stub\n"\
             "movq %%rsp,%%rdx \n"\
             "movq %[AsystemStack],%%rsp \n"\
             "movq %[Abuf],%%rdi \n"\
             "movq %[Ass],%%rsi \n"\
             "call finishStubRoutine \n"\
             : \
             : [Abuf] "r" (buf),\
               [Ass] "r" (ss),\
               [AsystemStack] "m" (systemStack)\
             : "rdi", "rdx", "rsi");} while (0)
#define resumeParent(sp, pc) do {\
	asm volatile("#switch to parent and resume\n" \
                     "movq %[Asp],%%rsp \n"\
                     "jmp *%[Apc] \n"\
	             : \
	             : [Asp] "r" (sp),\
		       [Apc] "r" (pc));} while (0)
#else 
#ifdef MACOS
#define switchToSysStackAndFreeAndResume(buf,sp,adr) do {\
asm volatile("movq %[Abuf],%%rdi \n"\
             "movq %[AsystemStack],%%rsp \n"\
             "pushq %[Asp] \n"\
             "pushq %[Aadr] \n"\
             "call _free \n"\
             "popq %%rbx \n"\
             "popq %%rax \n"\
             "movq %%rax,%%rsp \n"\
             "jmp *%%rbx \n"\
             :\
             : [Abuf] "r" (buf),\
               [Asp] "r" (sp),\
               [Aadr] "r" (adr),\
               [AsystemStack] "m" (systemStack)\
             : "rdi", "rbx", "rax");} while (0)
#else
#define switchToSysStackAndFreeAndResume(buf,sp,adr) do {\
asm volatile("movq %[Abuf],%%rdi \n"\
             "movq %[AsystemStack],%%rsp \n"\
             "pushq %[Asp] \n"\
             "pushq %[Aadr] \n"\
             "call free \n"\
             "popq %%rbx \n"\
             "popq %%rax \n"\
             "movq %%rax,%%rsp \n"\
             "jmp *%%rbx \n"\
             :\
             : [Abuf] "r" (buf),\
               [Asp] "r" (sp),\
               [Aadr] "r" (adr),\
               [AsystemStack] "m" (systemStack)\
             : "rdi", "rbx", "rax");} while (0)
#endif
#endif

#define restoreStackPointer(x) do { \
    asm volatile("movq %[sp],%%rsp" : : [sp] "r" (x) : "rsp"); } while (0)

#define getStackPointer(x) do { \
    asm volatile("movq %%rsp,%[output]" : [output] "=r" (x)); } while (0)

#define saveRegisters() do { \
    asm volatile("movq %%rcx,%[indexCX]   \n"\
                 "movq %%rdx,%[indexDX]   \n"\
                 "movq %%rbx,%[indexBX]   \n"\
                 "movq %%rbp,%[indexBP]   \n"\
                 "movq %%rsi,%[indexSI]   \n"\
                 "movq %%rdi,%[indexDI]   \n"\
                 "movq %%r8,%[indexR8]    \n"\
                 "movq %%r9,%[indexR9]    \n"\
                 "movq %%r10,%[indexR10]  \n"\
                 "movq %%r11,%[indexR11]  \n"\
                 "movq %%r12,%[indexR12]  \n"\
                 "movq %%r13,%[indexR13]  \n"\
                 "movq %%r14,%[indexR14]  \n"\
                 "movq %%r15,%[indexR15]  \n"\
                 "movq %%rax,%[indexAX]   \n"\
                 : [indexCX] "=m" (saveArea[0]),\
                   [indexDX] "=m" (saveArea[1]),\
                   [indexBX] "=m" (saveArea[2]),\
                   [indexBP] "=m" (saveArea[3]),\
                   [indexSI] "=m" (saveArea[4]),\
                   [indexDI] "=m" (saveArea[5]),\
                   [indexR8] "=m" (saveArea[6]),\
                   [indexR9] "=m" (saveArea[7]),\
                   [indexR10] "=m" (saveArea[8]),\
                   [indexR11] "=m" (saveArea[9]),\
                   [indexR12] "=m" (saveArea[10]),\
                   [indexR13] "=m" (saveArea[11]),\
                   [indexR14] "=m" (saveArea[12]),\
                   [indexR15] "=m" (saveArea[13]),\
                   [indexAX] "=m" (saveArea[14]));} while (0)

#define restoreRegisters() do { \
    asm volatile("movq %[indexCX],%%rcx   \n"\
                 "movq %[indexDX],%%rdx   \n"\
                 "movq %[indexBX],%%rbx   \n"\
                 "movq %[indexBP],%%rbp   \n"\
                 "movq %[indexSI],%%rsi   \n"\
                 "movq %[indexDI],%%rdi   \n"\
                 "movq %[indexR8],%%r8    \n"\
                 "movq %[indexR9],%%r9    \n"\
                 "movq %[indexR10],%%r10  \n"\
                 "movq %[indexR11],%%r11  \n"\
                 "movq %[indexR12],%%r12  \n"\
                 "movq %[indexR13],%%r13  \n"\
                 "movq %[indexR14],%%r14  \n"\
                 "movq %[indexR15],%%r15  \n"\
                 "movq %[indexAX],%%rax   \n"\
                 :\
                 : [indexCX] "m" (saveArea[0]),\
                   [indexDX] "m" (saveArea[1]),\
                   [indexBX] "m" (saveArea[2]),\
                   [indexBP] "m" (saveArea[3]),\
                   [indexSI] "m" (saveArea[4]),\
                   [indexDI] "m" (saveArea[5]),\
                   [indexR8] "m" (saveArea[6]),\
                   [indexR9] "m" (saveArea[7]),\
                   [indexR10] "m" (saveArea[8]),\
                   [indexR11] "m" (saveArea[9]),\
                   [indexR12] "m" (saveArea[10]),\
                   [indexR13] "m" (saveArea[11]),\
                   [indexR14] "m" (saveArea[12]),\
                   [indexR15] "m" (saveArea[13]),\
                   [indexAX] "m" (saveArea[14]));} while (0)

#define restoreRegistersExceptDI() do { \
    asm volatile("movq %[indexCX],%%rcx   \n"\
                 "movq %[indexDX],%%rdx   \n"\
                 "movq %[indexBX],%%rbx   \n"\
                 "movq %[indexBP],%%rbp   \n"\
                 "movq %[indexSI],%%rsi   \n"\
                 "movq %[indexR8],%%r8    \n"\
                 "movq %[indexR9],%%r9    \n"\
                 "movq %[indexR10],%%r10  \n"\
                 "movq %[indexR11],%%r11  \n"\
                 "movq %[indexR12],%%r12  \n"\
                 "movq %[indexR13],%%r13  \n"\
                 "movq %[indexR14],%%r14  \n"\
                 "movq %[indexR15],%%r15  \n"\
                 "movq %[indexAX],%%rax   \n"\
                 :\
                 : [indexCX] "m" (saveArea[0]),\
                   [indexDX] "m" (saveArea[1]),\
                   [indexBX] "m" (saveArea[2]),\
                   [indexBP] "m" (saveArea[3]),\
                   [indexSI] "m" (saveArea[4]),\
                   [indexR8] "m" (saveArea[6]),\
                   [indexR9] "m" (saveArea[7]),\
                   [indexR10] "m" (saveArea[8]),\
                   [indexR11] "m" (saveArea[9]),\
                   [indexR12] "m" (saveArea[10]),\
                   [indexR13] "m" (saveArea[11]),\
                   [indexR14] "m" (saveArea[12]),\
                   [indexR15] "m" (saveArea[13]),\
                   [indexAX] "m" (saveArea[14])\
                 : "rdi");} while (0)

#define getMsgPtrFromDI(msg) \
    asm volatile("movq %%rdi,%[aMsg]" : [aMsg] "=r" (msg) : : "rdi")


#define clobberCallerSave() \
    asm volatile("# make sure caller save regs are saved here\n"\
		: \
       		: \
		 : "rbp", "rbx", "r12", "r13", "r14", "r15")

// enq current stacklet on readyq.  called from inlet running on stacklet sp.
#define enQAndReturn() \
    asm volatile("# done with inlet, return from ULI\n"	\
		 "movq %%rsp,%%rdi \n"			\
    		 "movq $ER%=,%%rsi \n"		\
    	         "movq %[AsystemStack],%%rsp \n"	\
    		 "call finishEnQAndReturn\n"		\
		 "ER%=:				#return here after resume\n"	\
		 :					\
		 : [AsystemStack] "m" (systemStack)	\
		 : "rdi", "rsi")

// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
