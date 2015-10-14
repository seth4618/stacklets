#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include "assert.h"
#include "mythreads.h"

#ifndef OPTS_ON
// !!!!!!!!!!!!!!!!
// YOU BEST BE COMPILING WITH OPTS ON IF YOU DON'T DEFINE THIS!!
# define OPTS_ON 1
#endif

static int verbose = 0;
#ifdef VERBOSE_FREE
#define free(x) myfree(x, __LINE__)
static void myfree(void*p, int ln);
#endif

////////////////////////////////////////////////////////////////
// Each thread has this stuff

struct thread {
  Thread* next;			/* used for queing threads */
  int size;			/* size of stack */
  void* sp;			/* where sp starts */
  int state[20];		/* where we save registers */
  ThreadFunc start;		/* initial function to invoke */
  ThreadArg arg;		/* initial arg for start function */
  int detached;			/* set to 1 if detached */
  ThreadList* waiters;		/* threads waiting on this thread */
  Thread* waitingon;		/* thread we are waiting on while in idlequeue */
  ThreadId lastjoin;		/* last thread that we joined with */
  ThreadList* children;
};

struct threadlist {
  ThreadList* next;
  Thread* thread;
};

static void mythread_schedule_next(void);

#define isDetached(t) ((t)->detached & 1)
#define isDone(t) ((t)->detached & 2)
#define setDetached(t)  (t)->detached = 1
#define setDone(t)  (t)->detached |= 2;

////////////////////////////////////////////////////////////////
// get stack direction

static int stack_direction = 0;
typedef int (*fptr)();

// helpers to get stack direction even if optimizations all turned on
static int
check_stack_dir_helper1(int* gp, int* p, fptr parent, fptr me)
{
  int x;
  // first two should never happen
  if (parent == NULL) return 0;
  if (me == NULL) return 0;
  // see direction of stack growth
  if (verbose) fprintf(stderr, "gp:%p and parent:%p and me:%p\n", gp, p, &x);
  if ((unsigned int)gp > (unsigned int)p) return -1;
  return 1;
}

static int
check_stack_dir_helper(int* inparent, fptr me, fptr child)
{
  int c = *inparent;
  if (c == 0)
    return (*child)(inparent, &c, me, child);
  else {
    *inparent = c-1;
    return (*me)(inparent, me, child);
  }
}

static void
check_stack_dir(void)
{
  int c = 2;
  stack_direction = check_stack_dir_helper(&c, &check_stack_dir_helper, &check_stack_dir_helper1);
  if (verbose) fprintf(stderr, "Stack grows %s\n", stack_direction > 0 ? "up" : "down");
  if (stack_direction > 0) {printf("Does not function for this type of stack."); exit(0);}
}

////////////////////////////////////////////////////////////////
// thread scheduling

static Thread* readyThreads;		/* threads ready to run */
static Thread* idleThreads;		/* threads waiting on something */
static Thread* lastThread;		/* end of Q */

static void
init_q(void)
{
  readyThreads = NULL;
  idleThreads = NULL;
  lastThread = NULL;
}

static void
removeFromIdleQue(Thread* t)
{
  Thread* prev = NULL;
  Thread* idle = idleThreads;
  while (idle && idle != t) {
    prev = idle;
    idle = idle->next;
  }
  if (prev) {
    prev->next = idle->next;
  } else {
    idleThreads = NULL;
  }
}

static void
addToIdleQue(Thread* t)
{
  t->next = idleThreads;
  idleThreads = t;
}

static void
enqThread(Thread* t)
{
  if (verbose > 1) fprintf(stderr, "Enq: %p\n", t);
  if (lastThread) {
    lastThread->next = t;
  } else {
    readyThreads = t;
  }
  t->next = NULL;
  lastThread = t;
}

static Thread*
deqThread(void)
{
  if (readyThreads == NULL) return NULL;
  Thread* t = readyThreads;
  readyThreads = readyThreads->next;
  if (readyThreads == NULL) lastThread = NULL;
  void* p = (void*)(t->state[0]);
  if (verbose > 2) fprintf(stderr, "deq: %p  [%p < %p < %p]  %p\n", t, ((char*)t)+sizeof(Thread), p, t->sp, (*(void**)p));
  return t;
}

////////////////////////////////////////////////////////////////
// get everything setup here

void
mythreads_init(void)
{
  check_stack_dir();
  init_q();
}

////////////////////////////////////////////////////////////////
// asm stuff to resume a thread

 __attribute__ ((noinline)) 
static void
mythread_resume(Thread* t)
{
  asm volatile("movl (%0),%%esp; movl 4(%0),%%edi; movl 8(%0),%%esi; movl 20(%0),%%ebx; movl  24(%0),%%ebp; movl %1,%%eax"
	       :
	       :"r"(t->state), "r"(t)
	       :);
}

// return's NULL when invoked
// return's t when resumed
 __attribute__ ((noinline)) 
static Thread*
mythread_save(Thread* t)
{
  asm volatile("movl %%esp,(%0); movl %%edi,4(%0); movl %%esi,8(%0); movl %%edx,12(%0); movl %%ecx,16(%0); movl %%ebx,20(%0); movl %%ebp, 24(%0)"
      :
      :"r"(t->state)
      :);
  return 0;
}

// return's NULL when invoked
// return's t when resumed
__attribute__ ((noinline)) 
static Thread*
mythread_save_initial(Thread* t)
{
  asm volatile("addl %2,%0	  # get t->state into %0	\n\
                movl %%esp,(%0)   # save esp in t->state[0]	\n\
                movl %%edi,4(%0)  # save edi			\n\
		movl %%esi,8(%0)  # save esi			\n\
		movl %%edx,12(%0) # save edx			\n\
                movl %%ecx,16(%0) # save ecx			\n\
		movl %%ebx,20(%0) # save ebx			\n\
                movl %%ebp,24(%0) # save ebp			\n\
	        addl %3,%0	  # %0 <- &(t->sp)				\n\
		movl (%0),%0	  # %0 <- t->sp (top of stack for this thread)	\n\
		movl (%%esp),%%edx	# get esp[0] <- saved bp	\n\
		movl %%edx,(%0)	  # saved bp -> top of stack			\n\
		movl 4(%%esp),%%edx	# get esp[4] <- the return address	\n\
		movl %%edx,4(%0)	  # retadr -> top of stack			\n\
                xorl %0,%0	  # return NULL"
	       :"=r"(t)
	       :"0"(t), "i"((void*)&(t->state)-(void*)t), "i"((void*)&t->sp-(void*)&t->state)
	       :"%edx");
  return t;
}

////////////////////////////////////////////////////////////////
// start threading. (must have at least one thread on que

static jmp_buf alldone;
static int bufferarea = 512;
static int thread_default_stack_size = 8192*3;
static Thread* current_thread = NULL;

void
mythreads_start(void)
{
  if (verbose > 0) fprintf(stderr, "starting threads\n");
  if (setjmp(alldone)) {
    // all threads are done
    current_thread = NULL;
    return;
  }
  mythread_schedule_next();
}

static void
pushTL(ThreadList** headp, Thread* t)
{
  ThreadList* tl = calloc(1, sizeof(ThreadList));
  tl->next = *headp;
  tl->thread = t;
  *headp = tl;
}

static void
removeTL(ThreadList** headp, Thread* t)
{
  ThreadList* prev= NULL;
  ThreadList* tl = *headp;
  while (tl && tl->thread != t) {
    prev = tl;
    tl=tl->next;
  }
  assert(tl);
  if (prev)
    prev->next = tl->next;
  else
    *headp = NULL;
  free(tl);
}

static void
freeTL(ThreadList* head)
{
  ThreadList* nexttl;
  while (head) {
    nexttl = head->next;
    free(head);
    head = nexttl;
  }
}

static void
freeThread(Thread* t)
{
  if (t->waiters != NULL) {
    fprintf(stderr, "Thread %p exits while waiting on something\n", t);
    freeTL(t->waiters);
  }
  freeTL(t->children);
  free(t);
}

void
mythread_create(ThreadId* tid, void* x, ThreadFunc fptr, ThreadArg* arg)	
{
  if (verbose > 1) printf("Creating a thread\n");
  unsigned size = thread_default_stack_size;
  void* p = calloc(1, size+sizeof(Thread)+bufferarea);
  //void *p = malloc(size+sizeof(Thread)+bufferarea);
  //memset(p,0,size+sizeof(Thread)+bufferarea);
  Thread* t = (Thread*)p;
  t->size = size+sizeof(Thread);
  t->detached = 0;
  t->next = NULL;
  t->waiters = NULL;
  t->waitingon = NULL;
  if (current_thread) pushTL(&current_thread->children, t);
  // magic to get state setup
  t->sp = p+((stack_direction > 0)?sizeof(Thread):(size+sizeof(Thread)));
  t->start = fptr;
  t->arg = arg;
  // que up thread
  enqThread(t);
  // save threadid in caller
  if (tid != NULL) *tid = t;
  // save state setup
  Thread* newt = mythread_save_initial(t);
  // if this is initial return from savestate, start thread
  if (newt != NULL) {

    // !!!!!!!!!!!!!!!!
    // CALLING LIBC HERE CAN BE TROUBLESOME.  STACK IS SORT OF SEMI-SETUP
    // AT THIS POINT.  WAIT UNTIL WE CALL THE THREAD''s START FUNCTION AND
    // THEN IT WILL BE CLEAN
    // !!!!!!!!!!!!!!!!

    // we are about to start thread.  Stack is all setup
    current_thread = newt;
    (*newt->start)(newt->arg);
    // return here implies thread is done
    if (verbose > 1) fprintf(stderr, "returned from initial call\n");
    mythread_exit();
  } else {
    // return to caller if this is first time through
    // need to set proper stack for new thread first
    t->state[0] = (int)t->sp;
#if (OPTS_ON == 0)
    asm volatile("movl %0,%%eax; addl $60,%%eax; movl %%eax,(%0)" : : "r"(t->sp) : "%eax");
#endif
  }

}

static void
mythread_yield_and_wait()
{
  // put us on the idleList
  addToIdleQue(current_thread);
  Thread* t = mythread_save(current_thread);
  // check to see if we are returning from being scheduled. If so, return back to thread
  if (t) {
    // when the compiler is optimizing, it uses tail recursion for
    // this function, so we can get here.  We will go immediately to the join code
    assert(current_thread == t);
    return;
  }
  // nope, still here from saving old thread, so schedule a new one
  mythread_schedule_next();
}

void
mythread_yield(void)
{
  // save this thread on readylist
  enqThread(current_thread);
  // save state
  Thread* t = mythread_save(current_thread);
  // check to see if we are returning from being scheduled. If so, return back to thread
  if (t) return;
  // nope, still here from saving old thread, so schedule a new one
  mythread_schedule_next();
}

static void
mythread_schedule_next(void)
{
  Thread* t = deqThread();
  if (t == NULL) {
    if (verbose > 1) fprintf(stderr, "No more threads to run");
    longjmp(alldone, 1);
  }
  if (verbose > 0) fprintf(stderr, "Scheduling thread: %p\n", t);
  current_thread = t;
  mythread_resume(t);
}

ThreadId
mythread_myid(void)
{
  return current_thread;
}

// wait til who is done, or if who is NULL, for anyone to be done
ThreadId
mythread_join(ThreadId whoid)
{
  Thread* who = (Thread*)whoid;

  // first check dead threads
  if (who && isDone(who)) {
    setDetached(who);
    freeThread(who);
    return who;
  }
  if (who == NULL) {
    // grab any dead child that is not detached
    ThreadList* t;
    for (t=current_thread->children; t&&!(isDone(t->thread)&&!isDetached(t->thread)); t=t->next);
    if (t != NULL) {
      // should remove t from child list
      setDetached(t->thread);
      freeThread(t->thread);
      return t->thread;
    }
  }

  // now setup for live threads
  if (who) {
    // current_thread will wait on a specific thread, who
    pushTL(&who->waiters, current_thread);
  } 
  // indicate we are waiting on something
  current_thread->waitingon = who;
  mythread_yield_and_wait();
  // back from join, ready to go
  return current_thread->lastjoin;
}

// current_thread will run in detached mode
void
mythread_detach(void)
{
  setDetached(current_thread);
}

void
mythread_exit(void)
{
  // indicate we are done
  setDone(current_thread);

  // see if any threads are waiting on this thread
  int assigned = 0;
  if (current_thread->waiters) {
    ThreadList* w;
    ThreadList* nextw;
    // first see if any thread is waiting on this particular thread
    for (w=current_thread->waiters; w; w=nextw) {
      nextw = w->next;
      // move waiting thread to readyQ
      removeFromIdleQue(w->thread);
      w->thread->lastjoin = current_thread;
      enqThread(w->thread);
      assigned = 1;
      free(w);
    }
    current_thread->waiters = NULL;
  }

  if (!assigned && !isDetached(current_thread)) {
    // if not already joined with someone and
    // some other thread is waiting for anyone, go for it
    Thread* t;
    for (t=idleThreads; !assigned && t; t=t->next) {
      if (t->waitingon == NULL) {
	// t is waiting for anyone (SHOULDN'T I HAVE TO MAKE SURE CURRENT_THREAD IS A CHILD OF T?)
	removeFromIdleQue(t);
	t->lastjoin = current_thread;
	enqThread(t);
	assigned = 1;
      }
    }
  }
  // see if we should free this one
  if (isDetached(current_thread)||assigned) {
    // we need to reclaim memory
    freeThread(current_thread);
    current_thread = NULL;
  }
  // and schedule another thread
  mythread_schedule_next();
}

////////////////////////////////////////////////////////////////
// P & V
// completely bogus in a user threaded, non-preemptive package.
// Either we run on multiple cores in which case this code doesn't work
// or, we could use any old counter.


void
mythread_seminit(Semaphore* s, int cnt)
{
  assert(cnt > 0);
  s->cnt = cnt;
  s->waiters = NULL;
}

void
mythread_semwait(Semaphore* s)
{
  assert(s->cnt >= 0);
  if (s->cnt <= 0) {
    pushTL(&s->waiters, current_thread);
    mythread_yield_and_wait();
    removeTL(&s->waiters, current_thread);
  } else {
    s->cnt--;
  }
}

void
mythread_sempost(Semaphore* s, int cnt)
{
  assert(cnt > 0);
  s->cnt+=cnt;
  if (s->waiters != NULL) {
    Thread* t = s->waiters->thread;
    removeFromIdleQue(t);
    s->cnt--;			/* this decr happens here because we scheduled t, which uses up 1 cnt */
  }
}

#ifdef VERBOSE_FREE
# undef free

static void
myfree(void* p, int ln)
{
  fprintf(stderr, "Free %p @ %3d in %p\n", p, ln, current_thread);
  free(p);
}
#endif
