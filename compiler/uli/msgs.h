// @file msgs.h
#pragma once

#include "u_interrupt.h"

void myeui(int x);
void mydui(int x);
void checkUIMask(int x);

typedef struct _basicMessage BasicMessage;
struct _basicMessage {
    message systemMsg;
    int from;
    BasicMessage* next;
};

typedef struct {
    BasicMessage base;
    void (*func)(void*);
    void* arg;
    void* parentPC;
    void* parentSP;
} ForkMsg;

typedef struct {
    BasicMessage base;
} StealReqMsg;

typedef StealReqMsg StealFailMsg;

typedef struct {
    BasicMessage base;
    void* parentPC;
    void* parentSP;
} ReturnMsg;

#define MAX_MSG_SIZE	sizeof(ForkMsg)

// allocate a msg from poll big enough to handle any of the message defined here.
// fill in info from base (which sets ptr in base to returned ptr
void* getMsg(callback_t handler);

// called with nt==0 from each thread, called with nt==# of cores from thread 0
void initMsgs(int nt);

// call to free a msg buffer
void freeMsgBuffer(BasicMessage* msg);


extern __thread void** sysStackPtr;

// if running on normal 
#define myretuli() do { \
    void* sysStack = *(--sysStackPtr); \
    void* sysAdr = *(--sysStackPtr); \
    asm volatile("\t# return from user interrupt\n"\
    		 "movq %[handlerStack], %%rsp\n"    \
    		 "jmp *%[handlerIP]\n" \
		 : \
		 : [handlerStack] "r" (sysStack), \
		 [handlerIP] "r" (sysAdr));} while(0)
		 
#define setupULIret() do { \
	__label__ rethere,skiphere;		\
    void* thisSP;			\
    asm volatile("\t# return here to return from ULI\n" \
                 "movq %%rsp, %[sp] \n"  \
	     : [sp] "=r" (thisSP));	\
    labelhack(rethere); \
    labelhack(skipthere); \
    *(sysStackPtr++) = && rethere;	\
    *(sysStackPtr++) = thisSP;		\
    goto skipthere;  rethere: asm volatile("#return here"); return; skipthere: asm volatile("#continue here"); \
 } while(0)

#define setupULIret2() do { \
	__label__ rethere2,skiphere2;		\
    void* thisSP;			\
    asm volatile("\t# return here to return from ULI\n" \
                 "movq %%rsp, %[sp] \n"  \
	     : [sp] "=r" (thisSP));	\
    labelhack(rethere2); \
    labelhack(skipthere2); \
    *(sysStackPtr++) = && rethere2;	\
    *(sysStackPtr++) = thisSP;		\
    goto skipthere2;  rethere2: asm volatile("#return here"); return; skipthere2: asm volatile("#continue here"); \
 } while(0)

// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
