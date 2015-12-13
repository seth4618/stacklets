#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include "u_interrupt.h"
#include "mylock.h"

/* Global variables */
int count = 0;
int seconds = 4;
int interval = 1;
bool threads_join = 0;

/* Synchronization variables */
struct lock L;

void * increment_counter(void *arg)
{
    int i = 0;

    /*
     * Setup ULI stack.
     */
    void *my_stack = (void *) malloc(4096);
    if(my_stack == NULL) {
      exit(1);
    }
    SETUPULI(my_stack);

    for (i = 0; i < 100; ++i)
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

void usage(char *cmd)
{
    fprintf(stderr, "Usage: %s: -[h] -[p] <num_cores>", cmd);
    exit(1);
}

int main(int argc, char *argv[])
{
    pthread_t *threads;
    int i, rc, ncpus = 0;
    int c;
    char *cmd;

    while ((c = getopt(argc, argv, "p:h")) != -1) {
        cmd = argv[0];

        switch(c) {
            case 'p':
                ncpus = atoi(optarg);
                if (ncpus <= 0)
                    usage(cmd);
                break;
            case 'h':
                usage(cmd);
            default:
                break;
        }
    }

    if (!ncpus)
        ncpus = GET_NR_CPUS();

    INIT_ULI(ncpus);
    init_lock(&L);

    threads = calloc(ncpus, sizeof(pthread_t));
    if (!threads) {
        fprintf(stderr, "Could not allocate memory to threads\n");
        exit(1);
    }

    for (i=0; i<ncpus; i++) {
        rc = pthread_create(&threads[i], NULL, increment_counter, NULL);
        if (rc != 0) {
            fprintf(stderr,"Failed to create pthread\n");
            exit(1);
        }
    }
    sleep(seconds);
    // threads_join = 1;
    for (i=0; i<ncpus; i++) {
        rc = pthread_join(threads[i], NULL);
        if (rc < 0) {
            fprintf(stderr,"Failed to join pthread\n");
            exit(1);
        }
    }
    printf("Total count is %d\n",count);
    free(threads);
    exit(0);
}
