/**
 * Provides basic functionalities of a queue
 * implemented, using an array
 * @author KD
 * @date 2018-03-12
 **/

#include "queue.h"


/**
 * @name create_queue - Creates a queue.
 * @return the created queue.
 */
 Queue* create_queue(unsigned capacity){
	
	Queue* q = (Queue*)malloc(sizeof(Queue));
	q->capacity = capacity;
	q->head = 0;
	q->tail = 0;
	q->array = malloc(sizeof(Request_descriptor)*capacity);
	return q;
	
}

//Full queue.
int is_full(Queue* q){
	
	return (((q->head+1)%q->capacity == q->tail) && (q->tail== 0));
	
}


//Empty queue.
int is_empty(Queue* q){
	
	return (q->head == q->tail);
	
}


//adds the item to the q
void enqueue(Queue* q, Request_descriptor item){
	
	q->array[q->tail] = item;
	q->tail = (q->tail+1)% q->capacity;
	
}


//removes the head-th item from the Queue.
int dequeue(Queue* q){
	
	Request_descriptor* item = (Request_descriptor*)malloc(sizeof(Request_descriptor));
	*item = q->array[q->head];
	q->head = (q->head + 1)% q->capacity;
	free(item);
	return 0;
	
}

//returns the head-th item of the Queue.
Request_descriptor *get_head(Queue* q){
	
	return &(q->array[q->head]);
	
}