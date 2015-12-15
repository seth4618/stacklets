// @file spinlock.h
//
// spinlock implementation (for now use pthread mutex. Blech!)
#pragma once

#include <pthread.h>

#define USING_PTHREADS 1

extern pthread_mutexattr_t pthread_attr;
#define PTHREAD_LOCK_TYPE PTHREAD_MUTEX_ERRORCHECK

#if defined(USING_PTHREADS) && (USING_PTHREADS == 1)

void mySpinInitLock(pthread_mutex_t *mutex);
void mySpinLock(pthread_mutex_t *mutex);
void mySpinUnlock(pthread_mutex_t *mutex);
# define SpinLockType pthread_mutex_t

#else

 not done yet

#endif


// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
