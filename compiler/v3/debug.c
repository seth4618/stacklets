#include <stdarg.h>
#include "debug.h"

extern __thread int threadId;

void bp(void) {}

// helper functions
void die(char* str)
{
    fprintf(stderr, "Error: %s\n", str);
    exit(-1);
}

void
dprintLine(char* fmt, ...)
{
  va_list ap;
  char buffer[64];
  sprintf(buffer, "%d:%s", threadId, fmt);

  va_start(ap,fmt);
  vfprintf(stderr, buffer, ap);
  fflush(stderr);
  va_end(ap);
}

// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
