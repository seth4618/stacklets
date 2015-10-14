// lock free shared buffer, safe for multiple producers and multiple consumers

#include "sbuffree.h"
#define _GNU_SOURCE  
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <mythreads.h>

int idlep_count = 0;
int done_creating_threads = 0;

extern int verbose;
extern SharedBuffer sbuf;
extern int num_pthreads;
pthread_mutex_t mutex_idle_count;

void signal_exit_task();

void exit_task();

// Create an empty, bounded, shared FIFO buffer with n slots
void 
sbuf_init(int n)
{
  pthread_mutex_init(&mutex_idle_count,0);
  sbuf = calloc(1,sizeof(sbuf_t));
  sbuf->buf = calloc(n, sizeof(void*)); 
  sbuf->n = n;                         /* Buffer holds max of n items */
  sbuf->front = sbuf->rear = 0;        /* Empty buffer iff front == rear */
  sbuf->inserted = 0;
  sbuf->removed = 0;
  sbuf->trialRear = sbuf->trialFront = NULL;
}

// Clean up buffer sbuf
void 
sbuf_deinit(sbuf_t *sbuf)
{
  free(sbuf->buf);
}

// Insert item onto the rear of shared buffer sbuf
int 
sbuf_insert(void * item)
{
  int insertPoint;
  while (1) {
    // check to see if it is full. if full: return 0 and execute task immediately
    if ((sbuf->inserted - sbuf->removed) == sbuf->n) return 0;
    /* using atomic compare_and_swap instruction to make sure only 1 thread 
     * can insert into the buffer at a time
     */
    while ((__sync_val_compare_and_swap (&sbuf->trialRear, NULL, &insertPoint )) != NULL) pthread_yield();
    // make sure there is still sbuface
    if ((sbuf->inserted - sbuf->removed) == sbuf->n) {
    	sbuf->trialRear = NULL;
    	return 0;
    }
    insertPoint = sbuf->rear+1;
    if (sbuf->trialRear != &insertPoint) {
      sbuf->trialRear = NULL;
      continue;
    }
    sbuf->buf[insertPoint%(sbuf->n)] = (long) item; /* Potentially insert the item */
    sbuf->rear = insertPoint;
    break;
  }

  sbuf->inserted++;		      /* count items inserted */
  sbuf->trialRear = NULL;		/* indicate we don't need this anymore */
  if (verbose) 
    fprintf(stderr, "Leaving insert: Item:%p Pthread:%x Removed:%d Inserted:%d\n", item,(unsigned int)pthread_self(),sbuf->removed,sbuf->inserted);
  return 1;
}
/* $end sbuf_insert */

/* Remove and return the first item from buffer sbuf */
/* $begin sbuf_remove */
void* sbuf_remove()
{
  int removePoint;
  void* item;
  if (verbose)
    fprintf(stderr, "Entering remove: Pthread:%x Removed:%d Inserted:%d\n",(unsigned int)pthread_self(),sbuf->removed,sbuf->inserted);
  while (1) {
    removePoint = 0;
    /* Queue is empty */
    if (sbuf->removed == sbuf->inserted) {
      pthread_mutex_lock(&mutex_idle_count);
  	  if (idlep_count==(num_pthreads-1)) {
        /*The done_creating_threads flag is set when all the work is done and all the 
         *threads become idle
         */
        done_creating_threads = 1;
        fprintf(stderr,"Signalling to kill all threads: Idlepcount:%d Pthread:%x Removed:%d Inserted:%d\n",idlep_count,(unsigned int)pthread_self(),sbuf->removed,sbuf->inserted);
    	  signal_exit_task();
      }

      pthread_mutex_unlock(&mutex_idle_count);
    }

    pthread_mutex_lock(&mutex_idle_count);
    idlep_count++;
    pthread_mutex_unlock(&mutex_idle_count);

    while (sbuf->removed == sbuf->inserted) pthread_yield();
    
    pthread_mutex_lock(&mutex_idle_count);
    idlep_count--;
    pthread_mutex_unlock(&mutex_idle_count);
    
    /* using atomic compare_and_swap instruction to make sure only 1 thread 
     * can remove from the buffer at a time
     */
    while ((__sync_val_compare_and_swap (&sbuf->trialFront, NULL, &removePoint )) != NULL) pthread_yield();

    if (sbuf->removed == sbuf->inserted ) {
      sbuf->trialFront = NULL;
      continue;
    }
    
    removePoint = 1+sbuf->front;
    item = (void *)sbuf->buf[removePoint%(sbuf->n)];  /* Remove the item */

    if (sbuf->trialFront != &removePoint) continue;

    sbuf->front = removePoint;
    break;
  }
  sbuf->removed++;
  sbuf->trialFront = NULL;
  if (verbose)
    fprintf(stderr, "Leaving remove: Item:%p Pthread:%x Removed:%d Inserted:%d\n", item,(unsigned int)pthread_self(),sbuf->removed,sbuf->inserted);
  return item;
}
/* $end sbuf_remove */

/* Function :signal_exit_task
 * Description: create exit tasks for all the pthreads to exit
 * and insert them in the buffer.ss
 */
void signal_exit_task()
{
  done_creating_threads = 1;
  int i;
  ThreadId id[num_pthreads];
  for(i=0;i<num_pthreads;i++)
    mythread_create(&id[i], NULL, exit_task, NULL);
}

void exit_task()
{
  pthread_exit(NULL);
}

/* $end sbufc */