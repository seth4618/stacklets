/*Program to create and join n mythreads using 
 *multithreading 
 */

#include "mythreads.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/time.h>

int count;
void create_join(void *p);

/*
 * Function name: testitout
 * Description: This function is called from the main for calculating the  
 * fibonacci of the number represented by the input argument p.
 */
void
testitout(void* p) 
{
  fprintf(stderr, "main thread %p, %ld %x\n", mythread_myid(), (long)p,(unsigned int)pthread_self());
  /* Spawning and joining n threads*/
  long i,j,k;
  long num_mythreads=(long)p;
  ThreadId id[num_mythreads];
  
  //repeating k times
  for(k = 0; k < 1;k++) {
    for (i=0;i<num_mythreads;i++)
      mythread_create(&id[i], NULL, create_join, (void *)i);
    for (j=0;j<num_mythreads;j++)
      mythread_join(id[j]);
  }
  fprintf(stderr,"parent is done %x\n",(unsigned int)pthread_self());
  fprintf(stderr,"result is %d %x\n",count,(unsigned int)pthread_self());
  //exit(0);
}

/*
 * Function name: create_join
 * Description: This task updates the count variable everytime.
 */
void create_join(void *p) 
{
  //fprintf(stderr, "I am thread %p, %x, %p\n", mythread_myid(), (unsigned int)pthread_self(), p);
  
  
  //doing random work to make the code slower to check performance
  /*int i;
  int j;
  float a;
  for(i = 0; i < 2000 ; i++) {
    for (j=0; j<1000; j++) {
      a=0;
      a++;
      a=a/i;
      a=a*a;
      a=abs(a);
      a=a*a;
    }
  }*/
  count++;
  return;
  }

int
main(int argc, char** argv) 
{
  mythreads_init();
  if (argc != 3) {
      printf ("Arguments format: <program> <no_of_mythreads> <Num_pthreads>\n");
      return 0;
  }
  long n = atoi(argv[1]);
  int num_cores= atoi(argv[2]);

  mythread_create(NULL, NULL, testitout, (void *)n);
  mythreads_start(num_cores);

  return 0;
}