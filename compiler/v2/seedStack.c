// @file seedStack.c
#include "seedStack.h"
#include "stdlib.h"
#include "myfib.h"

int current_id;

void seedStackInit()
{
    seedDummyHead = (Seed *)calloc(1, sizeof(Seed));
}

Seed* initSeed(void* adr, void* sp)
{
    Seed* seed = calloc(1, sizeof(Seed));
    seed->adr = adr;
    seed->sp = sp;
    seed->id = current_id++;
    return seed;
}

void pushSeed(Seed* seed)
{
    Seed* origHead = seedDummyHead->next;
    seedDummyHead->next = seed;
    seed->prev = seedDummyHead;
    seed->next = origHead;
    if (origHead) origHead->prev = seed;
}

void popSeed(Seed* seed)
{
    seed->prev->next = seed->next;
    if (seed->next) seed->next->prev = seed->prev;
    free(seed);
}
