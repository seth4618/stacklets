#include "debug.h"

void bp(void) {}

// helper functions
void die(char* str)
{
    fprintf(stderr, "Error: %s\n", str);
    exit(-1);
}
