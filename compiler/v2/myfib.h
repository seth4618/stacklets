// @file myfib.h
//
// helper assembly functions
#pragma once
#include <stdlib.h>
#include <stdio.h>

#ifdef DEBUG
#define DEBUG_PRINT(fmt, args...)    fprintf(stderr, fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...)    /* Don't do anything in release builds */
#endif

typedef struct {
    int input;
    int output;
    int depth;
} Foo;