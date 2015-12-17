// @readyQ.c
#include "readyQ.h"
#include <stdlib.h>
#include "debug.h"

extern __thread int threadId;

static int readyQId;

// 1 readyQ per thread
static ReadyThreadHead* readyHead;

void readyQInit(int numThreads)
{
    readyHead = calloc(numThreads, sizeof(ReadyThreadHead));
}

void enqReadyQ(void* resumeAdr, void *stackPointer, void* arg, int tid)
{
    ReadyThread* ready = (ReadyThread *)calloc(1, sizeof(ReadyThread));
    ready->adr = resumeAdr;
    ready->sp = stackPointer;
    ready->arg = arg;
    ready->id = readyQId++;
    ReadyThreadHead* head = &readyHead[tid];
    if (head->back) {
        head->back->next = ready;
        head->back = ready;
    } else {
        head->front = ready;
        head->back = ready;
    }
}

void deqReadyQ(int tid)
{
    ReadyThreadHead* head = &readyHead[tid];
    ReadyThread* origFront = head->front;
    head->front = origFront->next;
    if (origFront->next == NULL) {
        head->back = NULL;
    }
    //DEBUG_PRINT("deqReadyQ with adr %p.\n", origFront->adr); XXX crash here
    free(origFront);
}

ReadyThread* peekReadyQ(int tid)
{
    ReadyThreadHead* head = &readyHead[tid];
    return head->front;
}

// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
