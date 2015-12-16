// @readyQ.c
#include "readyQ.h"
#include <stdlib.h>
#include "debug.h"

static int readyQId;

void readyQInit()
{
    readyDummyHead = (ReadyThreadHead *)calloc(1, sizeof(ReadyThreadHead));
}

void enqReadyQ(void* resumeAdr, void *stackPointer)
{
    ReadyThread* ready = (ReadyThread *)calloc(1, sizeof(ReadyThread));
    ready->adr = resumeAdr;
    ready->sp = stackPointer;
    ready->id = readyQId++;
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
    //DEBUG_PRINT("deqReadyQ with adr %p.\n", origFront->adr); XXX crash here
    free(origFront);
}


// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
