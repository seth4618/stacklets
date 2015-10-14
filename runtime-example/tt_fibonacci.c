/* Program to compute fibonacci of a number using multithreading */

#include "mythreads.h"
#include <stdlib.h>
#include <stdio.h>

/* 
 * Foo is a structure that will be passed as an argument to the fibonacci
 * function. Here, input represents the number for which the fibonacci is
 * to be computed and output represents the result 
*/
typedef struct {

  int input;
  int output;

} Foo;

/* Declaration of function prototypes */
void fibonacci(Foo* f);

/*
 * Function name: child
 * Description: This function is the thread routine and is called when 
 * a new thread is created. Here the argument p is a structure of type Foo.
 * p -> input : Number whose fibonacci is to be computed.
 * p -> output : Stores the result of the computation.
 */
void child(void *p)
{
  fprintf(stderr, "I am thread %p, %d\n", mythread_myid(), ((Foo *)p)->input);
  fibonacci(p);
  mythread_exit();
}

/*
 * Function name: testitout
 * Description: This function is called from the main for calculating the  
 * fibonacci of the number represented by the input argument p.
 */
void
testitout(void* p)
{
  fprintf(stderr, "main thread %p, %d\n", mythread_myid(), (int)p);
  /* Allocation heap space for storing input and output fields of fib(p) */
  Foo* a = (Foo *)calloc(1,sizeof(Foo));
  a->input = (int)p;
  /* Spawning a new thread for computing fib(p) */
  ThreadId id;
  mythread_create(&id, NULL, child, (void *)a);
  mythread_join(id);
  free(a);
  printf("parent is done\n");
  printf("result is %d",a->output);
  mythread_exit();
}

/*
 * Function name: fibonacci
 * Description: This function is called everytime the fibonacci of a number 
 * is to be computed.
 * p -> input : Number whose fibonacci is to be computed.
 * p -> output : Stores the result of the computation.
 */
void fibonacci(Foo* f)
{

  if(f->input <= 2) {
    f->output = 1;
  } else {
      /* Allocation heap space for storing input and output fields 
       * of fib(f-1) and fib(f-2)
       */
      Foo* a = (Foo *)calloc(1,sizeof(Foo));
      Foo* b = (Foo *)calloc(1,sizeof(Foo));
      a->input = f->input - 1;
      b->input = f->input - 2;
      ThreadId id1;
      ThreadId id2;
      /* Spawning a new thread for computing fib(f-1) */
      mythread_create(&id1, NULL, child, (void *)a);
      /* Spawning a new thread for computing fib(f-2) */
      mythread_create(&id2, NULL, child, (void *)b);
      /* Parent waits till the children compute the fibonacci */
      mythread_join(id1);
      mythread_join(id2);
      free(a);
      free(b);
     /* fib(f) = fib(f-1) + fib(f-2) */
      f->output = a->output + b->output;
    }

}

int
main(int argc, char** argv) {
  mythreads_init(); 
  int n = atoi(argv[1]);
  /* Computing fibonacci(n) */ 
  mythread_create(NULL, NULL, testitout, (void*)n);
  mythreads_start();
  return 0;
}
