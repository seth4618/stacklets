// @file spinlock.h
//
// spinlock implementation (for now use pthread mutex. Blech!)
#pragma once

#include <pthread.h>

#define USING_PTHREADS 1

#if defined(USING_PTHREADS) && (USING_PTHREADS == 1)
# define mySpinInitLock pthread_mutex_init
# define mySpinLock pthread_mutex_lock
# define mySpinUnlock pthread_mutex_unlock
#else
 not done yet
#endif


// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
