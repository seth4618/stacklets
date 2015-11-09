#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include "mylock.h"
#include "system.h"

/* Global variables */
int count = 0;
int seconds = 4;
int interval = 1;
bool threads_join = 0;

/* Synchronization variables */
struct lock L;

void * increment_counter(void *arg)
{
    int cpu = get_myid();
    unsigned long myid = (unsigned long)pthread_self();
    
    int i = 0;
    for (i = 0; i < 10; ++i)
    {
        POLL();
        mylock(&L);
        POLL();
        count++;
        POLL();
        myunlock(&L);
        POLL();
        sleep(interval);
                
    }
    POLL();
    sleep(2);   // Sleep to give time for other threads to send messages
    POLL();
    pthread_exit(NULL);
    return NULL;
}

int main(int argc, void *argv[])
{
    init_system();
    pthread_t threads[NUM_CORES];
    int i, rc;

    init_lock(&L);
    for (i=0; i<NUM_CORES; i++) {
        rc = pthread_create(&threads[i], NULL, increment_counter, NULL);
        if (rc != 0) {
            fprintf(stderr,"Failed to create pthread\n");
            exit(1);
        }
    }
    sleep(seconds);
    // threads_join = 1;
    for (i=0; i<NUM_CORES; i++) {
        rc = pthread_join(threads[i], NULL);
        if (rc < 0) {
            fprintf(stderr,"Failed to join pthread\n");
            exit(1);
        }
    }
    destroy_lock(&L);
    printf("Total count is %d\n",count);
    exit(0);
}
