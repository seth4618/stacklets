/* Program to compute fibonacci of a number using multithreading */

#include "mythreads.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "assert.h"
#include <fcntl.h>

/* 
 * Foo is a structure that will be passed as an argument to the fibonacci
 * function. Here, input represents the number for which the fibonacci is
 * to be computed and output represents the result 
*/
typedef struct {
  long input;
  long output;
} Foo;

/* Declaration of function prototypes */
void fibonacci(void* f);

/*
 * Function name: testitout
 * Description: This function is called from the main for calculating the  
 * fibonacci of the number represented by the input argument p.
 */
void
testitout(void* p) 
{
  fprintf(stderr, "main thread %p, %ld %x\n", mythread_myid(), (long)p,(unsigned int)pthread_self());
  /* Allocation heap space for storing input and output fields of fib(p) */
  Foo* a = (Foo *)calloc(1,sizeof(Foo));
  a->input = (long)p;
  /* Spawning a new thread for computing fib(p) */
  ThreadId id;
  mythread_create(&id, NULL, fibonacci, (void *)a);
  mythread_join(id);
  free(a);
  fprintf(stderr,"parent is done %x\n",(unsigned int)pthread_self());
  fprintf(stderr,"result is %ld %x\n",a->output,(unsigned int)pthread_self());
  //exit(0);
}

/*
 * Function name: fibonacci
 * Description: This function is called everytime the fibonacci of a number 
 * is to be computed.
 * p -> input : Number whose fibonacci is to be computed.
 * p -> output : Stores the result of the computation.
 */
void fibonacci(void* f) 
{
  Foo* foo = (Foo*)f;
  //fprintf(stderr, "I am thread %p, %ld, %x, RSP:%p\n", mythread_myid(), (long)foo->input, (unsigned int)pthread_self(),(void *)&foo);
  if(foo->input <= 2) {
    foo->output = 1;
  } 
  else {
  /* Allocation heap space for storing input and output fields 
   * of fib(f-1) and fib(f-2)
   */
    Foo* a = (Foo *)calloc(1,sizeof(Foo));
    Foo* b = (Foo *)calloc(1,sizeof(Foo));
    a->input = foo->input - 1;
    b->input = foo->input - 2;
    ThreadId id1;
    ThreadId id2;
    /* Spawning a new thread for computing fib(f-1) */
    mythread_create(&id1, NULL, fibonacci, (void *)a);
    /* Spawning a new thread for computing fib(f-2) */
    mythread_create(&id2, NULL, fibonacci, (void *)b);
    /* Parent waits till the children compute the fibonacci */
    mythread_join(id1);
    mythread_join(id2);
    /* fib(f) = fib(f-1) + fib(f-2) */
    foo->output = a->output + b->output;
    free((void *)a);
    free((void *)b);
	}
}

int
main(int argc, char** argv) 
{
  mythreads_init();
  if (argc != 3) {
    printf ("Arguments format: <program> <n> <Num_cores>\n");
    return 0;
  }
  long n = atoi(argv[1]);
  int num_cores= atoi(argv[2]); 

  /* Computing fibonacci(n) */ 
  mythread_create(NULL, NULL, testitout, (void*)n);
  mythreads_start(num_cores);
  return 0;
}