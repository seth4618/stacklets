#ifndef QUEUE_H
#define QUEUE_H QUEUE_H
#include "spinlock.h"
#include "malloc.h"
#include "stddef.h"

#define CACHE_LINE_SIZE                   64

#define r_align(n, r)                     (((n) + (r) - 1) & -(r))
#define cache_align(n)                    r_align(n , CACHE_LINE_SIZE)
#define pad_to_cache_line(n)              (cache_align(n) - (n))
typedef void (*callback_t)(void*);
typedef struct message_t
{
	callback_t callback;
	void *p;
}message;

typedef struct node_t
{
	message *msg;
	struct node_t *prev;	
	struct node_t *next;	
}node;

typedef struct queue_t
{
	node *head;
	node *tail;
	spinlock_t lock;
    char    pad[pad_to_cache_line(2 * sizeof(void *) +
                        sizeof(spinlock_t))];
}queue;

void init_queue(queue *q);
int is_empty(queue *q);
void enqueue(queue *q, message *msg);
message* dequeue(queue *q);

#endif
