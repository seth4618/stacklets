// @file seedStack.c
#include "seedStack.h"
#include "stdlib.h"
#include "myfib.h"

int current_id;

void seedStackInit()
{
    seedDummyHead = (Seed *)calloc(1, sizeof(Seed));
}

Seed* initSeed(void* adr, void* sp, void (*routine)(void *), void* argv,
               void* parentStubBase)
{
    Seed* seed = calloc(1, sizeof(Seed));
    seed->adr = adr;
    seed->sp = sp;
    seed->routine = routine;
    seed->argv = argv;
    seed->id = current_id++;
    seed->parentStubBase = parentStubBase;
    return seed;
}

void pushSeed(Seed* seed)
{
    DEBUG_PRINT("Push seed[id %d] %d\n", seed->id, ((Foo *)(seed->argv))->input);
    Seed* origHead = seedDummyHead->next;
    seedDummyHead->next = seed;
    seed->prev = seedDummyHead;
    seed->next = origHead;
    if (origHead) origHead->prev = seed;
}

void popSeed(Seed* seed)
{
    DEBUG_PRINT("Pop seed[id %d] %d\n", seed->id, ((Foo *)(seed->argv))->input);
    seed->prev->next = seed->next;
    if (seed->next) seed->next->prev = seed->prev;
    free(seed);
}
