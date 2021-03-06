// @file stacklet.h
#pragma once

#include <stdint.h>

typedef uint64_t Registers[16];

typedef struct {
    void* stubRoutine;
    void* parentPC;
    void* parentSP;
    void* parentStubBase;
} Stub;

extern void* systemStack;
extern void* stubBase;

#define SYSTEM_STACK_SIZE   8192
#define STACKLET_SIZE       81920

void stackletInit();
void systemStackInit();
void stubRoutine();
void stackletFork(void* parentPC, void* parentSP, void (*func)(void*), void* arg);
void suspend();
void yield(void);

#define labelhack(x) \
    asm goto("" : : : : x)

#define suspendStub() do {\
asm volatile("movq %[sysStack], %%rsp \n"\
             "call suspend \n"\
             :\
             : [sysStack] "m" (systemStack));} while (0)

#define stackletForkStub(parentPC, parentSP, func, arg) do {\
asm volatile("movq %[AparentPC], %%rdi \n"\
             "movq %[AparentSP], %%rsi \n"\
             "movq %[Afunc], %%rdx \n"\
             "movq %[Aarg], %%rcx \n"\
             "movq %[sysStack], %%rsp \n"\
             "call stackletFork \n"\
             :\
             : [AparentPC] "r" (parentPC),\
               [AparentSP] "r" (parentSP),\
               [Afunc] "r" (func),\
               [Aarg] "r" (arg),\
               [sysStack] "m" (systemStack)\
             : "rdi", "rsi", "rdx", "rcx");} while (0)

#define switchAndJmpWithArg(sp, adr, arg) do {\
asm volatile("movq %[Aarg], %%rdi \n"\
             "movq %[Asp], %%rsp \n"\
             "jmp *%[Aadr] \n"\
             :\
             : [Asp] "r" (sp),\
               [Aadr] "r" (adr),\
               [Aarg] "r" (arg)\
             : "rdi");} while (0)

#define switchAndJmp(sp,adr) do {\
asm volatile("movq %[Asp],%%rsp \n"\
             "jmp *%[Aadr] \n"\
             :\
             : [Asp] "r" (sp),\
               [Aadr] "r" (adr));} while(0)

#define switchToSysStackAndFreeAndResume(buf,sp,adr,parentSB) do {\
asm volatile("movq %[AparentSB], %[AStubBase] \n"\
             "movq %[Abuf],%%rdi \n"\
             "movq %[AsystemStack],%%rsp \n"\
             "pushq %[Asp] \n"\
             "pushq %[Aadr] \n"\
             "call free \n"\
             "popq %%rbx \n"\
             "popq %%rax \n"\
             "movq %%rax,%%rsp \n"\
             "jmp *%%rbx \n"\
             : [AStubBase] "=m" (stubBase) \
             : [Abuf] "r" (buf),\
               [Asp] "r" (sp),\
               [Aadr] "r" (adr),\
               [AsystemStack] "m" (systemStack),\
               [AparentSB] "r" (parentSB)\
             : "rdi");} while (0)

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
