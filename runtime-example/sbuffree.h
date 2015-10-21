#ifndef __SBUF_H__
#define __SBUF_H__

// private - Not to be touched by users of this package

typedef struct {
  long *buf;                 /* Buffer array */         
  int n;                     /* Maximum number of slots */
  volatile int front;        /* buf[(front+1)%n] is first item */
  volatile int rear;         /* buf[rear%n] is last item */
  volatile int* trialFront;
  volatile int* trialRear;
  volatile int inserted;	 /* # of items inserted into buffer */
  volatile int removed;	     /* # of items removed from the buffer */
} sbuf_t;

// public API

typedef sbuf_t* SharedBuffer;

// call before you use the buffer
void sbuf_init(int n);

// call after you are done to free memory
void sbuf_deinit(SharedBuffer sp);

// insert an item, return when inserted
int sbuf_insert(void* item);

// remove an item, only return when there is an item to return
void* sbuf_remove();

#endif