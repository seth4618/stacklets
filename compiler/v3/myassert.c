#include <assert.h>
#include "myassert.h"
#include "debug.h"

void 
assertFail(char* test, char* file, int line, char* fmt, ...)
{
  va_list ap;
  va_start(ap,fmt);

  char buffer[128];
  sprintf(buffer, "%s:%d:Assert Failure:'%s'. %s", file, line, test, fmt);
  vdprintLine(buffer, ap);
  assert(0);
  va_end(ap);
}

