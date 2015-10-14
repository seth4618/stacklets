/* Program to compute fibonacci of a number using multithreading */

#include "mythreads.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>


long token;
ThreadId id[100];

void token_pass(void *p);
/*
 * Function name: testitout
 * Description: This function is called from the main for calculating the  
 * fibonacci of the number represented by the input argument p.
 */
void
testitout(void* p) {
  fprintf(stderr, "main thread %p, %ld %x\n", mythread_myid(), (long)p,(unsigned int)pthread_self());
  /* Spawning a new thread for computing fib(p) */
  
  long i,j;
  token=0;
  long num_mythreads=(long)p;
  
  for (i=0; i<num_mythreads;i++)
    mythread_create(&id[i], NULL, token_pass,(void*)num_mythreads);

  for (j=0; j<num_mythreads;j++)
    mythread_join(id[j]);

  printf("parent is done %x\n",(unsigned int)pthread_self());
  printf("result is %ld %x\n",token,(unsigned int)pthread_self());
  fflush(stdout);
  // mythread_exit();
}

/*
 * Function name: token_pass
 * Description: The mythread waits here till its turn comes to increment 
 * the value of token. keeps running till the value of token reaches 20.
 */
void token_pass(void *num_mythreads) {
  while (token<20)
  {
    long num_threads=(long)num_mythreads;
    ThreadId ID=mythread_myid();
    long index;
    index= token % num_threads;

    if (id[index]==ID)
      token++;
  }
  return;
}

int
main(int argc, char** argv) {
  clock_t start, end;
  double time_spent;

  start=clock();
  
  mythreads_init();
  if (argc != 3) {
      printf ("Arguments format: <program> <no_of_mythreads> <Num_pthreads>\n");
      return 0;
    }
    long n = atoi(argv[1]);
    int num_cores= atoi(argv[2]);
    /* Computing fibonacci(n) */ 
    mythread_create(NULL, NULL, testitout, (void *)n);
    mythreads_start(num_cores);

    end=clock();
    time_spent=(double)(end-start)/CLOCKS_PER_SEC;
    printf("TIME TAKEN=  %lf  \n",time_spent);
    return 0;
}