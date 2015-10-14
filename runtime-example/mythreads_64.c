#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include "assert.h"
#include "mythreads.h"
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include "sbuffree.h"
#include <errno.h>


#ifndef COMPILING_OPTION
// !!!!!!!!!!!!!!!!
// 0 when compiling with -O0 flag
#define COMPILING_OPTION 0
#endif

int verbose = 0;
#ifdef VERBOSE_FREE
#define free(x) myfree(x, __LINE__)
static void myfree(void*p, int ln);
#endif

//////////// get stack direction ///////////////////////////////

static int stack_direction = 0;
typedef long (*fptr)();
void (*func_ptr)();

// helpers to get stack direction even if optimizations all turned on
static long
check_stack_dir_helper1(long* gp, long* p, fptr parent, fptr me) 
{
  long x;
  // first two should never happen
  if (parent == NULL) return 0;
  if (me == NULL) return 0;
  // see direction of stack growth
  if (verbose > 1) fprintf(stderr, "gp:%p and parent:%p and me:%p\n", gp, p, &x);
  if ((long)gp > (long)p) return -1;
  return 1;
}

static long
check_stack_dir_helper(long* inparent, fptr me, fptr child) 
{
  long c = *inparent;
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
  long c = 2;
  stack_direction = check_stack_dir_helper(&c, &check_stack_dir_helper, &check_stack_dir_helper1);
  if (verbose > 1) fprintf(stderr, "Stack grows %s\n", stack_direction > 0 ? "up" : "down");

  if (stack_direction > 0) { 
    if (verbose > 1) fprintf(stderr,"Does not function for this type of stack");
    exit(0);
  }
}

////////// Declaration of globals /////////////////////////////

/* Pointer to shared buffer */
SharedBuffer sbuf;

int num_pthreads;

///////// Declaration of static variables /////////////////////

/* Threads waiting on something */
static Thread* idleThreads;  
/* Pointer to struct containing pthread specific information */
static pthread_current_thread** current_thread_list; 

static int bufferarea = 512;
static int thread_default_stack_size = 8192*7;

///////// Declaration of extern variables /////////////////////
extern int done_creating_threads;

///////// Declaration of mutexes //////////////////////////////
pthread_mutex_t mutex_idle;

static void
init_q(void) 
{
  idleThreads = NULL;
  sbuf_init(num_of_threads);
  pthread_mutex_init(&mutex_idle,0);
}

void
removeFromIdleQue(Thread* t) 
{
  if (verbose > 1)
    fprintf(stderr, "In idle: %lx: F:%d R:%d\n", pthread_self(), sbuf->front, sbuf->rear);
  Thread* prev = NULL;
  Thread* idle = idleThreads;
  pthread_mutex_lock(&mutex_idle);
  while (idle && idle != t) {
    prev = idle;
    idle = idle->next;
  }
  if (prev) {
    prev->next = idle->next;
  } 
  else {
    idleThreads = NULL;
  }
  pthread_mutex_unlock(&mutex_idle);
}

void
addToIdleQue(Thread* t) 
{
  pthread_mutex_lock(&mutex_idle);
  t->next = idleThreads;
  idleThreads = t;
  pthread_mutex_unlock(&mutex_idle);
}


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
  asm volatile("movq (%0),%%rsp; movq 8(%0),%%rbx; movq 16(%0),%%rbp; movq 24(%0),%%r12; movq 32(%0),%%r13; movq 40(%0),%%r14; movq 48(%0),%%r15; movq %%rdi,%%rax;"
         :
         :"r"(t->state), "r"(t)
         :);
}

// return's NULL when invoked
// return's t when resumed
 __attribute__ ((noinline)) 
Thread*
mythread_save(Thread* t) 
{
  asm volatile("movq %%rsp,(%0); movq %%rbx,8(%0); movq %%rbp, 16(%0); movq %%r12,24(%0); movq %%r13,32(%0); movq %%r14,40(%0); movq %%r15,48(%0);"
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
  asm volatile("addq %2,%0          # get t->state into %0  \n\
                movq %%rsp,(%0)     # save rsp in t->state[0] \n\
                movq %%rbx,8(%0)    # save rbx     \n\
                movq %%rbp,16(%0)   # save ebp      \n\
                movq %%r12,24(%0)   # save r12      \n\
                movq %%r13,32(%0)   # save r13      \n\
                movq %%r14,40(%0)   # save r14      \n\
                movq %%r15,48(%0)   # save r15      \n\
                addq %3,%0          # %0 <- &(t->sp)        \n\
                movq (%0),%0        # %0 <- t->sp (top of stack for this thread)  \n\
                movq (%%rsp),%%rdx  # get rsp[0] <- saved bp  \n\
                movq %%rdx,(%0)     # saved bp -> top of stack      \n\
                movq 8(%%rsp),%%rdx # get rsp[4] <- the return address  \n\
                movq %%rdx,8(%0)    # retadr -> top of stack             \n\
                xorq %0,%0          # return NULL"
                :"=r"(t)
                :"0"(t), "i"((void*)&(t->state)-(void*)t), "i"((void*)&(t->sp)-(void*)&t->state)
                :"%rdx");
  return t;
}

void
mythreads_start(int num_cores) 
{
  num_pthreads=num_cores; //taken user input

  if (verbose > 1)
   fprintf(stderr, "starting threads\n");

  pthread_t tid[num_pthreads];
  long i;
  current_thread_list = calloc(num_pthreads,sizeof(pthread_current_thread*));

  for(i = 0 ; i < num_pthreads ; i++)
    current_thread_list[i] = calloc(1,sizeof(pthread_current_thread));

  /* create number of pthreads requested by the user */
  for (i=0; i<num_pthreads; i++)
    pthread_create(&tid[i],NULL,thread,(void *)i);   
  
  for(i = 0 ; i < num_pthreads ; i++)
    pthread_join(tid[i],NULL);

  return; 
}

void *thread(void *vargp) 
{
  if (verbose > 1)
   fprintf(stderr,"thread id=%x\n",(unsigned int)pthread_self());
  unsigned long index = (unsigned long) vargp;
  /* Populating the tid parameter pertaining to each pthread */
  current_thread_list[index] -> tid = pthread_self();
  mythread_schedule_next();
  return NULL;
}

void
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

void
freeThread(Thread* t) 
{
  if (t->waiters != NULL) {
    if (verbose > 1)
     fprintf(stderr, "Thread %p exits while waiting on something\n", t);
    freeTL(t->waiters);
  }
  freeTL(t->children);
  free(t);
}


 /*
 * Function name: get_current_pthread
 * Description: This function returns a struct pertaining to the
 * currently running pthread. The struct contains the pthread's tid, stack RSP and 
 * current_thread parameter.
 */

volatile pthread_current_thread* get_current_pthread()
{
  int i = 0;
  for(;i < num_pthreads; i++)
    if(current_thread_list[i] -> tid == pthread_self()) {
      return current_thread_list[i];
    }
  return NULL;
}


void
mythread_create(ThreadId* tid, void* x, ThreadFunc fptr, ThreadArg* arg) 
{
  if (verbose > 1)
   fprintf(stderr,"Creating a thread\n");
  unsigned size = thread_default_stack_size;
  void* p = calloc(1,size+sizeof(Thread)+bufferarea);
  Thread* t = (Thread*)p;
  t->size = size+sizeof(Thread);
  t->detached = 0;
  t->next = NULL;
  t->waiters = NULL;
  t->waitingon = NULL;
  
  /* Do not push children if inserting exit task */
  if(!done_creating_threads)
    if (get_current_pthread()) pushTL(&(get_current_pthread() -> current_thread)->children, t);
  // magic to get state setup
  t->sp = p+((stack_direction > 0)?sizeof(Thread):(size+sizeof(Thread)));
  t->start = fptr;
  t->arg = arg;
  t->insert_fail = 0;
  pthread_mutex_init(&(t->lock),0);

  // save threadid in caller
  if (tid != NULL) *tid = t;

  // save state setup
  register Thread* newt = mythread_save_initial(t);

  // if this is initial return from savestate, start thread
  if (newt != NULL) {

    // !!!!!!!!!!!!!!!!
    // CALLING LIBC HERE CAN BE TROUBLESOME.  STACK IS SORT OF SEMI-SETUP
    // AT THIS POINT.  WAIT UNTIL WE CALL THE THREAD''s START FUNCTION AND
    // THEN IT WILL BE CLEAN
    // !!!!!!!!!!!!!!!!

    // we are about to start thread.  Stack is all setup
     //get_current_pthread() -> current_thread = newt;
    (*newt->start)(newt->arg);
    // return here implies thread is done
    //if (verbose > 1) fprintf(stderr, "returned from initial call\n");
     mythread_exit();
  } 
  else {
    // return to caller if this is first time through
    // need to set proper stack for new thread first
    t->state[0] = t->sp;
    if (verbose) 
      fprintf(stderr,"Inserting new thread: %p,%x,%p,%d,RSP:%p\n",t,(unsigned int)pthread_self(),t->arg,sbuf->inserted,(void *)&size);

    /* Check if buffer is full. If yes, execute task immediately else insert item into the buffer and return */
    if (!(sbuf_insert(t))) {
      // pushing waiters
      pushTL(&t->waiters, get_current_pthread()->current_thread);
      // make insert_fail 1 
      t->insert_fail = 1;
      volatile pthread_current_thread* current = NULL;
      current = get_current_pthread();
      Thread* save_t = mythread_save(get_current_pthread()->current_thread);
      // check to see if we are returning from being scheduled. If so, return back to thread
  
      if (save_t) {
        // when the compiler is optimizing, it uses tail recursion for
        // this function, so we can get here. 
        assert(get_current_pthread()->current_thread == save_t);
        return;
      }
      
      current->current_thread = t;
      mythread_resume(t);
    }
  } 
}

ThreadId
mythread_join(ThreadId whoid) 
{
  //the register keyword is used to store the output only in registers, and not on stack
  register Thread* who = (Thread*)whoid;

  if (verbose) 
    fprintf(stderr,"In join: %p,%x,%d\n",whoid,(unsigned int)pthread_self(),isDone(whoid));

  pthread_mutex_lock(&who->lock);

  // first check dead threads
  if (who && isDone(who)) {
    setDetached(who);
    pthread_mutex_unlock(&who->lock);
    freeThread(who);
    return who;
  }

  /*if (who == NULL) {
    // grab any dead child that is not detached
    ThreadList* t;
    for (t=get_current_pthread()->current_thread->children; t&&!(isDone(t->thread)&&!isDetached(t->thread)); t=t->next);
    if (t != NULL) {
      // should remove t from child list
      setDetached(t->thread);
      freeThread(t->thread);
      return t->thread;
    }
  }*/

  // now setup for live threads
  if (who) {
    // current_thread will wait on a specific thread, who
    pushTL(&who->waiters, get_current_pthread()->current_thread);
  } 

  /*if (who && isDone(who)) {
    setDetached(who);
    freeThread(who);
    return who;
  }*/

  // indicate we are waiting on something
  get_current_pthread()->current_thread->waitingon = who;
  mythread_yield_and_wait(who);
  // back from join, ready to go
  return get_current_pthread()->current_thread->lastjoin;
}

void
mythread_exit(void) 
{
  if (verbose) 
    fprintf(stderr,"In exit: %p,%x\n",get_current_pthread()->current_thread,(unsigned int)pthread_self());

  pthread_mutex_lock(&(get_current_pthread()->current_thread->lock));

  setDone(get_current_pthread()->current_thread);

  //int assigned=0;
  register Thread *parent = NULL;

  // check if item was not inserted into the buffer
  if (get_current_pthread()->current_thread->insert_fail) {
    ThreadList* w;
    w=get_current_pthread()->current_thread->waiters;
    get_current_pthread()->current_thread->waiters = NULL;
    setDone(get_current_pthread()->current_thread);
    pthread_mutex_unlock(&(get_current_pthread()->current_thread->lock));
    get_current_pthread()->current_thread = w->thread;
    // Resume parent
    mythread_resume(w->thread);
  }

  // see if any threads are waiting on this thread
  if (get_current_pthread()->current_thread->waiters) {
    ThreadList* w;
    ThreadList* nextw;
    // first see if any thread is waiting on this particular thread
    for (w=get_current_pthread()->current_thread->waiters; w; w=nextw) {
      nextw = w->next;
      // move waiting thread to readyQ
      // removeFromIdleQue(w->thread);
      w->thread->lastjoin = get_current_pthread()->current_thread;
      // If buffer is full, resume parent immediately
      if(!sbuf_insert(w->thread))
        parent = w->thread;

      //assigned = 1;
      //free(w);
    }
    get_current_pthread()->current_thread->waiters = NULL;
  }


  // Switch to pthread stack
  asm volatile ("movq (%0),%%rsp;" : : "r" ( get_current_pthread()->pthread_sp));

  pthread_mutex_unlock(&(get_current_pthread()->current_thread->lock));

  get_current_pthread()->current_thread = NULL;

  if(parent) {
    get_current_pthread()->current_thread = parent;
    mythread_resume(parent);
  }

  // and schedule another thread
  mythread_schedule_next();
}


void
mythread_schedule_next(void) 
{
  Thread* t = NULL;

  t = sbuf_remove();

  if (t == NULL) {
    if (verbose > 1)
     fprintf(stderr,"No more threads to run : %x\n",(unsigned int)pthread_self());
     return;
  }

  if (verbose) 
    fprintf(stderr, "Scheduling thread: %p %x\n", t,(unsigned int)pthread_self());

  get_current_pthread()->current_thread = t;
 
  //decrementing for -O2 flag to counter the addition to rsp
  /*#if (COMPILING_OPTION == 1)
  asm volatile ("subq $0x10,%rsp;");
  #endif*/
  
  // Save stack pointer
  asm volatile("movq %%rsp,(%0);" : : "r"(get_current_pthread() -> pthread_sp));
  mythread_resume(t);
}

void
mythread_yield_and_wait(Thread* who) 
{
  volatile pthread_current_thread* current = NULL;
  
  current = get_current_pthread();
  
  // put us on the idleList
  //addToIdleQue(current->current_thread);

  Thread* t = mythread_save(current->current_thread);
  // check to see if we are returning from being scheduled. If so, return back to thread
  
  if (t) {
    // when the compiler is optimizing, it uses tail recursion for
    // this function, so we can get here.  We will go immediately to the join code
    pthread_mutex_lock(&who->lock);
    freeThread(who);
    assert(get_current_pthread()->current_thread == t);
    pthread_mutex_unlock(&who->lock);
    return;
  }

  // Switch to pthread stack
  asm ("movq (%0),%%rsp;" : : "r" ( current->pthread_sp));

  pthread_mutex_unlock(&who->lock);
   
  // nope, still here from saving old thread, so schedule a new one
  mythread_schedule_next();
}

ThreadId
mythread_myid(void) 
{
  return get_current_pthread()->current_thread;
}



// current_thread will run in detached mode
void
mythread_detach(void) 
{
  setDetached(get_current_pthread()->current_thread);
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
  Thread* who = NULL;
  assert(s->cnt >= 0);
  if (s->cnt <= 0) {
    pushTL(&s->waiters, get_current_pthread()->current_thread);
    mythread_yield_and_wait(who);
    removeTL(&s->waiters, get_current_pthread()->current_thread);
  } 
  else {
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
    s->cnt--;     /* this decr happens here because we scheduled t, which uses up 1 cnt */
  }
}

#ifdef VERBOSE_FREE
# undef free

static void
myfree(void* p, int ln) 
{
  free(p);
}
#endif
