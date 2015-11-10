// @readyQ.c
#include "readyQ.h"
#include <stdlib.h>

void readyQInit()
{
    readyDummyHead = (ReadyThreadHead *)calloc(1, sizeof(ReadyThreadHead));
}

void enqReadyQ(void *stackPointer)
{
    ReadyThread* ready = (ReadyThread *)calloc(1, sizeof(ReadyThread));
    ready->sp = stackPointer;
    if (readyDummyHead->back) {
        readyDummyHead->back->next = ready;
        readyDummyHead->back = ready;
    } else {
        readyDummyHead->front = ready;
        readyDummyHead->back = ready;
    }
}

void deqReadyQ()
{
    ReadyThread* origFront = readyDummyHead->front;
    readyDummyHead->front = origFront->next;
    if (origFront->next == NULL) {
        readyDummyHead->back = NULL;
    }
    free(origFront);
}
