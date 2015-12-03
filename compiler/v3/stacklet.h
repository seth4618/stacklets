// @file stacklet.h
#pragma once

#include <stdint.h>
#include <pthread.h>

//#define MACOS

extern __thread int threadId;
extern int numberOfThreads;

typedef uint64_t Registers[16];

typedef struct {
    void* stubRoutine;
    void* parentPC;
    void* parentSP;
} Stub;

extern __thread void* systemStack;
extern pthread_mutex_t readyQLock;

#define SYSTEM_STACK_SIZE   (8192*2)
#define STACKLET_SIZE       81920

void stackletInit(int numThreads);
void* systemStackInit();
void stubRoutine();
void stackletFork(void* parentPC, void* parentSP, void (*func)(void*), void* arg, int tid);
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
#define stackletForkStub(parentPC, parentSP, func, arg) do {\
asm volatile("movq %[AparentPC], %%rdi \n"\
             "movq %[AparentSP], %%rsi \n"\
             "movq %[Afunc], %%rdx \n"\
             "movq %[Aarg], %%rcx \n"\
             "movq %[sysStack], %%rsp \n"\
             "call _stackletFork \n"\
             :\
             : [AparentPC] "r" (parentPC),\
               [AparentSP] "r" (parentSP),\
               [Afunc] "r" (func),\
               [Aarg] "r" (arg),\
               [sysStack] "m" (systemStack)\
             : "rdi", "rsi", "rdx", "rcx");} while (0)
#else
#define stackletForkStub(parentPC, parentSP, func, arg, tid) do {	\
asm volatile("\t#calling stackletFork\n"\
	     "movq %[AparentPC], %%rdi \n"\
             "movq %[AparentSP], %%rsi \n"\
             "movq %[Afunc], %%rdx \n"\
             "movq %[Aarg], %%rcx \n"\
             "movl %[TIDarg], %%r8d \n"\
             "movq %[sysStack], %%rsp \n"\
             "call stackletFork \n"\
             :\
             : [AparentPC] "r" (parentPC),\
               [AparentSP] "r" (parentSP),\
               [Afunc] "r" (func),\
               [Aarg] "r" (arg),\
               [TIDarg] "r" (tid),\
               [sysStack] "m" (systemStack)\
             : "rdi", "rsi", "rdx", "rcx", "r8");} while (0)
#endif

#define switchAndJmpWithArg(sp, adr, arg) do {\
asm volatile("movq %[Aarg], %%rdi \n"\
             "movq %[Asp], %%rsp \n"\
             "jmp *%[Aadr] \n"\
             :\
             : [Asp] "r" (sp),\
               [Aadr] "r" (adr),\
               [Aarg] "r" (arg)\
             : "rdi");} while (0)

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
#define switchAndJmp(sp,adr) do {		\
asm volatile("movq %[Asp],%%rsp \n"\
             "jmp *%[Aadr] \n"\
             :\
             : [Asp] "r" (sp),\
               [Aadr] "r" (adr));} while(0)

// right after restoring registers this is needed
// see switchAndJmp
//#define recoverParentTID(tid) \
//    asm volatile("movl %%eax,%[Itid]"\
//		 : [Itid] "=r" (tid)	\
//                 :\
//		 : "eax")

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


// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
