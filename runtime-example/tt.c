//
// compile with mythreads as follows:
//
// gcc -m32 -Wall -static -o tt tt.c mythreads.c
//
#include "mythreads.h"
#include <stdio.h>

void child(void* p)
{
  fprintf(stderr, "I am thread %p, %d\n", mythread_myid(), (int)p);
  mythread_exit();
}

void
testitout(void* p)
{
  int i, cnt = (int)p;
  fprintf(stderr, "main thread %p, %d\n", mythread_myid(), (int)p);
  for (i=0; i<cnt; i++) mythread_create(NULL, NULL, child, (void*)i);
  printf("start joining\n");
  for (i=0; i<cnt; i++) mythread_join(NULL);
  printf("parent is done\n");
  mythread_exit();
}

int
main() {
  mythreads_init();
  mythread_create(NULL, NULL, testitout, (void*)20);
  mythreads_start();
  return 0;
}
