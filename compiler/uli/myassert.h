// @file myassert.h
#pragma once

#include <stdarg.h>

#define myassert(test, msg, args...)   do { if (!(test)) assertFail(#test, __FILE__, __LINE__, msg, ## args); } while(0)

void assertFail(char* test, char* file, int line, char* fmt, ...);

// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
