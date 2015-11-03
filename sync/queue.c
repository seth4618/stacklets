#include "queue.h"
#include "assert.h"

void init_queue(queue *q) {
	if (q == NULL) return; 
	q -> head = NULL;
	q -> tail = NULL;
	spinlock_init(&q -> lock);
}

int is_empty(queue *q) {

    if (q == NULL) return 1;

	spinlock_lock(&q -> lock);
	int result = q->tail==NULL;
	spinlock_unlock(&q -> lock);
	return result;
}
// requires that n is in the q
node *queue_delete(queue *q, node *n) {
	if (q == NULL || n == NULL) return NULL;
	if (n -> prev != NULL) {
		n -> prev -> next = n -> next;
	} else if (q -> head == n) {
		q -> head = n -> next;
	} else {
		return NULL;
	}

	if (n -> next != NULL) {
		n -> next -> prev = n -> prev;
	} else if (q -> tail == n) {
		q -> tail = n -> prev;
	} else {
		return NULL;
	}
	return n;
}


// redundatn funcitons 
message *dequeue(queue *q) {
	spinlock_lock(&q -> lock);
	node *n = queue_delete(q, q -> head);
	if (n == NULL)
	{
	spinlock_unlock(&q -> lock);

		return NULL;
	} 
		message *msg = n -> msg;
		free(n);
	spinlock_unlock(&q -> lock);

		return msg;
	
}

void enqueue(queue *q, message *msg) {

	node *n = (node *)malloc(sizeof(struct node_t));
	if (n == NULL) return;

    /* First node to be inserted */
    if (q == NULL) {
        q = (queue *)malloc(sizeof(struct queue_t));
        q->tail = NULL;
        q->head = NULL;
    }
	spinlock_lock(&q -> lock);

	n -> msg = msg;
	if (q -> tail == NULL) // q is empty
	{
		q -> head = q -> tail = n;
		n -> prev = n -> next = NULL;
	} else {
		q -> tail -> next = n;
		n -> prev = q -> tail;
		n -> next = NULL;
		q -> tail = n;
	}
	spinlock_unlock(&q -> lock);
	
}
