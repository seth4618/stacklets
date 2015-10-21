#include <pthread.h>
// Copyright (c) 2013, Seth Goldstein and Carnegie Mellon University
//
// very fragile user level non-preemptive threading package running on
// a single core only
//
// has been tested on ubuntu 12.10, gcc version 4.7.2, 32-bit
// with verbose msgs, Only works with statically linked libc
//
// sample compile flags:
//
// CFLAGS = -m32 -Wall
// if debugging package, use -static linking and also probably use -fno-stack-protector
//
// If you were compiling testmythreads.c and it used mythreads:
//
// gcc -m32 -Wall -static -o testmythreads testmythreads.c mythreads.c
//
// super simple sample usage
//
//  #include "mythreads.h"
//  #include "stdio.h"
//  
//  void child(void* p)
//  {
//    printf("I am thread %d\n", (int)p);
//  }
//  
//  void
//  testitout(void* p)
//  {
//    int i, cnt = (int)p;
//    for (i=0; i<cnt; i++) mythread_create(NULL, NULL, child, (void*)i);
//    for (i=0; i<cnt; i++) mythread_join(NULL);
//  }
//  
//  int
//  main() {
//    mythreads_init();
//    mythread_create(NULL, NULL, testitout, (void*)5);
//    mythreads_start();
//    return 0;
//  }


#ifndef _MYTHREAD_H_
#define _MYTHREAD_H_

#define isDetached(t) ((t)->detached & 1)
#define isDone(t) ((t)->detached & 2)
#define setDetached(t)  (t)->detached = 1
#define setDone(t)  (t)->detached |= 2

/* Size of buffer */
#define num_of_threads 50

/* thread creation, etc. interface */

typedef struct thread Thread;
typedef Thread* ThreadId;
typedef void (*ThreadFunc)(void* p);
typedef void* ThreadArg;
typedef unsigned long int pthread_t;
typedef struct threadlist ThreadList;
typedef struct pthread_current_thread pthread_current_thread;

////////////////////////////////////////////////////////////////
// Each thread has this stuff

struct thread {
  Thread* next;     /* used for queing threads */
  int size;     /* size of stack */
  void* sp;     /* where sp starts */
  void* state[20];    /* where we save registers */
  ThreadFunc start;   /* initial function to invoke */
  ThreadArg arg;    /* initial arg for start function */
  int detached;     /* set to 1 if detached */
  ThreadList* waiters;    /* threads waiting on this thread */
  Thread* waitingon;    /* thread we are waiting on while in idlequeue */
  ThreadId lastjoin;    /* last thread that we joined with */
  ThreadList* children;
  int insert_fail;
  pthread_mutex_t lock;
};

struct threadlist {
  ThreadList* next;
  Thread* thread;
};

struct pthread_current_thread {
  pthread_t tid;
  Thread* current_thread;
  void *pthread_sp[1];
};

// sempahore interface

typedef struct semaphore Semaphore;
struct semaphore {
  int cnt;
  ThreadList* waiters;
};


// call before you do anything else
void mythreads_init(void);
// call after you fork your first thread to get things going
void mythreads_start(int num_cores);
// create a new thread
void mythread_create(ThreadId* tid, void* x, ThreadFunc fptr, ThreadArg* arg);
// detach the currently running thread
void mythread_detach(void);
// exit from the currently running thread
void mythread_exit(void);
// join with another thread (or, if whoid==NULL with any non-detached child)
ThreadId mythread_join(ThreadId whoid);
// yield up processor to another thread
void mythread_yield(void);
void mythread_yield_and_wait(Thread* who);
void pushTL(ThreadList** headp, Thread* t);
void freeThread(Thread* t);
void mythread_schedule_next(void);
void mythread_schedule_next_exit(void *exit_thread);
void addToIdleQue(Thread* t);
void removeFromIdleQue(Thread* t);
inline void mythread_schedule_next(void);
void *thread(void *vargp);
void exit_task();
volatile pthread_current_thread* get_current_pthread();

 __attribute__ ((noinline)) 
Thread*
mythread_save(Thread* t); 
// get my threadid
ThreadId mythread_myid(void);

void mythread_seminit(Semaphore* s, int cnt);
void mythread_semwait(Semaphore* s);
void mythread_sempost(Semaphore* s, int cnt);

#endif
