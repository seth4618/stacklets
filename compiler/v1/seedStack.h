// @file seedStack.h
//
// seed stack
#pragma once

typedef struct Seed {
    void* adr;
    void* sp;
    void (*routine)(void *);
    void* argv;
    int activated;
    int joinCounter;
    struct Seed* prev;
    struct Seed* next;
    int id;
    void* parentStubBase;
} Seed;

Seed* seedDummyHead;
void seedStackInit();
Seed* initSeed(void* adr, void* sp, void (*routine)(void *), void* argv,
               void* parentStubBase);
void pushSeed(Seed* seed);
void popSeed(Seed* seed);
