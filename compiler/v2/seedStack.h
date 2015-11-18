// @file seedStack.h
//
// seed stack
#pragma once

typedef struct Seed {
    void* adr;
    void* sp;
    struct Seed* prev;
    struct Seed* next;
    int id;
} Seed;

Seed* seedDummyHead;
void seedStackInit();
Seed* initSeed(void* adr, void* sp);
void pushSeed(Seed* seed);
void popSeed(Seed* seed);
