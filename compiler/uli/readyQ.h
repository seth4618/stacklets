// @file readyQ.h
//
// ready queue
#pragma once

typedef struct ReadyThread {
    void* adr;
    void* sp;
    void* arg;
    int id;
    struct ReadyThread* next;
} ReadyThread;

typedef struct ReadyThreadHead{
    struct ReadyThread* front;
    struct ReadyThread* back;
} ReadyThreadHead;

void readyQInit(int numThreads);
void enqReadyQ(void* resumeAdr, void *stackPointer, void* arg, int tid);
void deqReadyQ(int tid);
ReadyThread* peekReadyQ(int tid);


// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
