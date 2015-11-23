// @file readyQ.h
//
// ready queue
#pragma once

typedef struct ReadyThread {
    void* adr;
    void* sp;
    int id;
    struct ReadyThread* next;
} ReadyThread;

typedef struct ReadyThreadHead{
    struct ReadyThread* front;
    struct ReadyThread* back;
} ReadyThreadHead;

ReadyThreadHead* readyDummyHead;

void readyQInit();
void enqReadyQ(void* resumeAdr, void *stackPointer);
void deqReadyQ();
