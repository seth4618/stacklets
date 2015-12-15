#include "spinlock.h"
#include "myassert.h"
#include <errno.h>

pthread_mutexattr_t pthread_attr;

void
setupLocks(void)
{
  int retval;
  retval = pthread_mutexattr_settype(&pthread_attr, PTHREAD_LOCK_TYPE);
  myassert(retval ==  0, "couldn't set attr: %d", retval);
}

void 
mySpinInitLock(pthread_mutex_t *mutex)
{
  int retval;
  retval = pthread_mutex_init(mutex, &pthread_attr);
  myassert(retval == 0, "Could not init mutex: %d\n", retval);
}

void 
mySpinLock(pthread_mutex_t *mutex)
{
  int retval = pthread_mutex_lock(mutex);
  myassert(retval == 0, "Failure to lock mutex: %d [%s]\n", retval, strerror(retval));
}

void 
mySpinUnlock(pthread_mutex_t *mutex)
{
  int rval = pthread_mutex_trylock(mutex);
  if (rval != EBUSY) {
    dprintLine("Trying to unlock %p but it wasn't locked? %d\n", mutex, rval);
    myassert(0, "blah");
  }
  int retval = pthread_mutex_unlock(mutex);
  myassert(retval == 0, "Failure to unlock mutex: %d [%s]\n", retval, strerror(retval));
}


// Local Variables:
// mode: c           
// c-basic-offset: 4
// End:
