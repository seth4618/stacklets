// @file readyQ.h
//
// ready queue
#pragma once

typedef struct ReadyThread {
    void* sp;
    struct ReadyThread* next;
} ReadyThread;

typedef struct ReadyThreadHead{
    struct ReadyThread* front;
    struct ReadyThread* back;
} ReadyThreadHead;

ReadyThreadHead* readyDummyHead;

void readyQInit();
void enqReadyQ(void *stackPointer);
void deqReadyQ();
