#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "assert.h"
#include "mythreads.h"

#define mythread_exit() 

int left=10;

void
thread(void* p)
{
  fputs("Entered\n", stderr);
  int x = (int)p;
  int y = 2;
  fprintf(stderr, "Entered thread %d with %d\n", (int)mythread_myid(), x);
  mythread_yield();
  fprintf(stderr, "back in thread %d with %d and %d\n", (int)mythread_myid(), x, y++);
  mythread_yield();
  fprintf(stderr, "back in thread %d with %d and %d\n", (int)mythread_myid(), x, y++);
  if (left) {
    ThreadId tid3;
    left--;
    mythread_create(&tid3, NULL, thread, (void*)left);
  }
  mythread_exit();
}

void
tree(void* p)
{
  int x = (int)p;
  int i;

  // msg
  for (i=0; i<x; i++) fputs(" ", stderr);
  fprintf(stderr, "Starting %p\n", mythread_myid());

  // spawn child
  if (x <3) {
    for (i=0; i<x; i++) fputs(" ", stderr);
    fprintf(stderr, "About to spawn child %p\n", mythread_myid());
    ThreadId tid;
    mythread_create(&tid, NULL, tree, (void*)(x+1));
    // wait for child
    ThreadId child = mythread_join(tid);
    for (i=0; i<x; i++) fputs(" ", stderr);
    fprintf(stderr, "Joined with child(%p) in %p\n", child, mythread_myid());
  }
  for (i=0; i<x; i++) fputs(" ", stderr);
  fprintf(stderr, "Exiting %p\n", mythread_myid());
  mythread_exit();
}

void
worker(void* p)
{
  int x = (int)p;
  fprintf(stderr, " in worker: %p (%d)\n", mythread_myid(), x);
  mythread_exit();
}

void
checknull(void* p)
{
  int x = (int)p;
  int i;
  for (i=0; i<x; i++) {
    ThreadId tid;
    mythread_create(&tid, NULL, worker, (void*)(i));
  }
  for (i=0; i<x; i++) {
    ThreadId child = mythread_join(NULL);
    fprintf(stderr, "joined with %p\n", child);
  }
  mythread_exit();
}

Semaphore mutex;          /* Semaphore that protects counter */
volatile int collected;
int spawned;

void
worker1(void* p)
{
  mythread_detach();
  mythread_semwait(&mutex);
  collected++;
  mythread_sempost(&mutex, 1);
  mythread_exit();
}

void
checkcollect(void* p)
{
  int nt = (int)p;

  spawned = 0;
  collected = 0;

  mythread_seminit(&mutex, 1);  /* mutex = 1 */
  int i;
  for (i=0; i<nt; i++) {
    ThreadId tid1;
    mythread_create(&tid1, NULL, worker1, 0);
    spawned++;
    mythread_yield();
  }
  while ((collected < spawned)) {
    printf("%d spawned, %d collected\n", spawned, collected);
    mythread_yield();
  }
  printf("%d spawned, %d collected\n", spawned, collected);
  mythread_exit();
}

int
main(int argc, char** argv)
{
  mythreads_init();

  if (argc != 2) die("Need an argument: %d\n", argc);
  int nt = atoi(argv[1]);
  ThreadId tid;
  mythread_create(&tid, NULL, checkcollect, (void*)(nt));
  mythreads_start();
  //  exit(0);

  int i;
  int n = 5;
  for (i=0; i<n; i++) {
    ThreadId tid;
    mythread_create(&tid, NULL, tree, (void*)0);
  }
  mythreads_start();
  printf("After threads 1\n");

  mythread_create(&tid, NULL, checknull, (void*)(n*2));
  mythreads_start();
  printf("After threads 2\n");
  exit(0);

  ThreadId tid1;
  ThreadId tid2;
  mythread_create(&tid1, NULL, thread, (void*)1);
  fprintf(stderr, "Created thread: %d\n", (int)tid1);
  mythread_create(&tid2, NULL, thread, (void*)2);
  fprintf(stderr, "Created thread: %d\n", (int)tid2);
  mythreads_start();
  printf("After threads\n");
  exit(0);
}
