#include<stdio.h>
#include<stdlib.h>


typedef struct node{
	
	int fd; //used for file descriptor
	struct timeval start_time; //start time of the connection 
	
}Request_descriptor;

typedef struct queue{
	
	int head,tail;
	unsigned capacity;
	Request_descriptor* array; 
	
}Queue;


 Queue* create_queue(unsigned capacity);
 int is_full(Queue* q);
 int is_empty(Queue* q);
 void enqueue(Queue* q, Request_descriptor item);
 int dequeue(Queue* q);
 Request_descriptor* get_head(Queue* q);