#ifndef QUEUE_H
#define QUEUE_H QUEUE_H
#include "spinlock.h"
#include "malloc.h"
#include "stddef.h"

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
}queue;

void init_queue(queue *q);
int is_empty(queue *q);
void enqueue(queue *q, message *msg);
message* dequeue(queue *q);

#endif
