#include "util.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void die(char* fmt, ...)
{
  va_list ap;

  va_start(ap,fmt);
  fprintf(stderr, "Fatal Error:");
  vfprintf(stderr, fmt, ap);
  fflush(stderr);
  va_end(ap);
  exit(-1);
}

void debug(char* fmt, ...)
{
  va_list ap;

  va_start(ap,fmt);
  fprintf(stderr, "debug:");
  vfprintf(stderr, fmt, ap);
  fflush(stderr);
  va_end(ap);
}

char* 
sprintfa(char* buffer, char* fmt, ...)
{
  va_list ap;

  va_start(ap,fmt);
  vsprintf(buffer, fmt, ap);
  int len = strlen(buffer);
  va_end(ap);
  return buffer+len;
}

////////////////////////////////////////////////////////////////
// dynamic array code

static void
ArrayGrow(Array* p)
{
  assert(p->size > 1);
  p->data = realloc(p->data, p->size*2*sizeof(void*));
  p->size = p->size*2;
  return;
}

Array* 
ArrayAllocate(int startsize)
{
  if (startsize < 2) startsize = 2;
  Array* p = calloc(sizeof(Array), 1);
  p->data = calloc(sizeof(void*), startsize);
  p->size = startsize;
  p->length = 0;
  return p;
}

void 
ArrayFree(Array* p)
{
  free(p->data);
  free(p);
}

void 
ArrayAdd(Array* p, void* elem)
{
  if (p->size == p->length) ArrayGrow(p);
  p->data[p->length++] = elem;
}

void* 
ArrayGet(Array* p, int idx)
{
  while (idx > p->size) {
    ArrayGrow(p);
  }
  while (idx > p->length) {
    p->data[p->length++] = NULL;
  }
  return p->data[idx];
}

void 
ArrayPut(Array* p, int idx, void* elem)
{
  while (idx > p->size) {
    ArrayGrow(p);
  }
  while (idx > p->length) {
    p->data[p->length++] = NULL;
  }
  p->data[idx] = elem;
  if (idx >= p->length) p->length=idx+1;
}

int 
ArrayLength(Array* p)
{
  return p->length;
}

// Local Variables:
// tab-width: 4
// indent-tabs-mode: nil
// End:

