// @file debug.c
#pragma once

#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG
#define DEBUG_PRINT(fmt, args...)    fprintf(stderr, fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...)    /* Don't do anything in release builds */
#endif

#define EMBED_BREAKPOINT bp()
void bp(void);

// helper functions
void die(char* str);

void dprintLine(char* fmt, ...);
void vdprintLine(char* fmt, ...);

#ifdef DLINE
# define DPL(fmt, args...)	dprintLine(fmt, ## args)
#else
# define DPL(fmt, args...)	
#endif

// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
