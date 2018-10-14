/* server.c

   Sample code of 
   Assignment L1: Simple multi-threaded key-value server
   for the course MYY601 Operating Systems, University of Ioannina 

   (c) S. Anastasiadis, G. Kappes 2016
    Modified by KD 2018
*/


#include <signal.h>
#include <sys/stat.h>
#include<pthread.h>
#include <stdbool.h>
#include "utils.h"
#include "kissdb.h"
#include "queue.h" //request queue

#define MY_PORT                 6767
#define BUF_SIZE                1160
#define KEY_SIZE                 128
#define HASH_SIZE               1024
#define VALUE_SIZE              1024
#define MAX_PENDING_CONNECTIONS   10
#define QUEUE_SIZE				  100
#define MAX_CONSUMER_NUM		  8//number of consumers

// Definition of the operation type.
typedef enum operation {
  PUT,
  GET
} Operation; 


// Definition of the request.
typedef struct request {
  Operation operation;
  char key[KEY_SIZE];  
  char value[VALUE_SIZE];
} Request;


// Definition of the database.
KISSDB *db = NULL;

//consumer threads 
pthread_t *cons_threads;	

//mutex variable for the request queue 
pthread_mutex_t q_mutex;

//condition variables used for empty and full request queue 
pthread_cond_t non_empty_queue,non_full_queue;


//request queue
Queue* request_queue;


//mutex variable for the key-value db 
pthread_mutex_t no_writer_mutex,no_reader_mutex;

//condition variables for the db
pthread_cond_t no_writer,no_reader;


//statistics variables
double total_waiting_time = 0.0,total_service_time = 0.0;
int completed_requests=0;

//mutexes for the statistics variables
pthread_mutex_t comp_requests_mutex,waiting_time_mutex,service_time_mutex;


//mutex variable for terminating threads
pthread_mutex_t stop_mutex;

//condition variable used to terminate worker threads
pthread_cond_t stop;

//used to notify that program is about to terminate
bool finished = false;

//counters for the readers and writers of db
int reader_count=0,writer_count=0;
//mutexes of the counters
pthread_mutex_t writer_count_mutex,reader_count_mutex;


/**
 * @name parse_request - Parses a received message and generates a new request.
 * @param buffer: A pointer to the received message.
 *
 * @return Initialized request on Success. NULL on Error.
 */
Request *parse_request(char *buffer) {
  char *token = NULL;
  Request *req = NULL;
  
  // Check arguments.
  if (!buffer)
    return NULL;
  
  // Prepare the request.
  req = (Request *) malloc(sizeof(Request));
  memset(req->key, 0, KEY_SIZE);
  memset(req->value, 0, VALUE_SIZE);

  // Extract the operation type.
  token = strtok(buffer, ":");    
  if (!strcmp(token, "PUT")) {
    req->operation = PUT;
  } else if (!strcmp(token, "GET")) {
    req->operation = GET;
  } else {
    free(req);
    return NULL;
  }
  
  // Extract the key.
  token = strtok(NULL, ":");
  if (token) {
    strncpy(req->key, token, KEY_SIZE);
  } else {
    free(req);
    return NULL;
  }
  
  // Extract the value.
  token = strtok(NULL, ":");
  if (token) {
    strncpy(req->value, token, VALUE_SIZE);
  } else if (req->operation == PUT) {
    free(req);
    return NULL;
  }
  return req;
}


void print_current_time(struct timeval* tv);

/**
 * @name process_request - Process a client request.
 * @param socket_fd: The accept descriptor.
 *
 */
void process_request(const int socket_fd) {

    char response_str[BUF_SIZE], request_str[BUF_SIZE];
    int numbytes = 0;
    Request *request = NULL;
	
    // Clean buffers.
    memset(response_str, 0, BUF_SIZE);
    memset(request_str, 0, BUF_SIZE);
    
    // receive message.
    numbytes = read_str_from_socket(socket_fd, request_str, BUF_SIZE);
			  
	
    // parse the request.
    if (numbytes) {
      request = parse_request(request_str);
      if (request) {

        switch (request->operation) {
         
		 case GET:
			
			pthread_mutex_lock(&reader_count_mutex);
			++reader_count;
			
			/*Block new writers*/
			if (reader_count == 1)
				while(pthread_mutex_trylock(&no_writer_mutex))
					;
			
			pthread_mutex_unlock(&reader_count_mutex);
			
		   // Read the given key from the database.
            if (KISSDB_get(db, request->key, request->value))
              sprintf(response_str, "GET ERROR\n");
            else{
              sprintf(response_str, "GET OK: %s\n", request->value);
			 
			}
			
			/*Check if there is at least one reader in the store.*/
			pthread_mutex_lock(&reader_count_mutex);
			
			/*no reader, notify a pending writer.*/
			if(--reader_count==0){
				
				pthread_mutex_unlock(&no_writer_mutex);
				pthread_mutex_lock(&no_reader_mutex);
				pthread_cond_signal(&no_reader);
				pthread_mutex_unlock(&no_reader_mutex);
				
			}
			
			pthread_mutex_unlock(&reader_count_mutex);
			
			
            break;
         
		  case PUT:
		    
			
			pthread_mutex_lock(&no_reader_mutex);
		
			pthread_mutex_lock(&reader_count_mutex);
			
			/*Wait if there is at least one reader.*/
			while (reader_count>0)
				pthread_cond_wait(&no_reader,&no_reader_mutex);
		
			pthread_mutex_lock(&no_writer_mutex);
			pthread_mutex_unlock(&no_reader_mutex);
			pthread_mutex_unlock(&reader_count_mutex);
			
			pthread_mutex_lock(&writer_count_mutex);
			/*Wait, if there is at least one writer.*/
			while(writer_count==1)
				pthread_cond_wait(&no_writer,&no_writer_mutex);
			
		
			if (writer_count==0)
				writer_count=1;
			else
				fprintf(stderr,"\nERROR:only one writer is allowed.\n");
			
            // Write the given key/value pair to the database.
            if (KISSDB_put(db, request->key, request->value)) 
              sprintf(response_str, "PUT ERROR\n");
            else{
              sprintf(response_str, "PUT OK\n");
			 
			}
			
			writer_count=0;
			
			/*no writer, notify a pending writer or release lock for pending readers .*/
			if (writer_count==0){
				pthread_cond_signal(&no_writer);
				pthread_mutex_unlock(&no_writer_mutex);
			}
			pthread_mutex_unlock(&writer_count_mutex);	
			
			
            break;
          default:
            // Unsupported operation.
            sprintf(response_str, "UNKOWN OPERATION\n");
        }
		
		struct timeval* start_time = (struct timeval*)malloc(sizeof(struct timeval));
		gettimeofday(start_time,NULL);
		
		//Print the service time.
		fprintf(stdout,"(Info)Response to socket with fd %d on ",socket_fd);
		print_current_time(start_time);
		printf("\n");
		free(start_time);
		
        // Reply to the client.
		write_str_to_socket(socket_fd, response_str, strlen(response_str));
		
			
        if (request)
          free(request);
        request = NULL;
		
		
        return;
      }
    }
    // Send an Error reply to the client.
    sprintf(response_str, "FORMAT ERROR\n");
    write_str_to_socket(socket_fd, response_str, strlen(response_str));
}


/**
 *@name handle_request - used by consumer threads to process requests.
 *@param rd: the descriptor of the request to be processed.
 *@return 0 on Success.
 */
int handle_request(Request_descriptor *rd){
	
	
	Request_descriptor* temp = (Request_descriptor*)malloc(sizeof(Request_descriptor));
	*temp = *rd;
	int fd;

	fd = temp->fd;

	process_request(fd);

	free(temp);
	return 0;
	

}


/**
 * @name get_time_interval - Computes time elapsed between the two given arguments. 
 * @param t1,t2: two timeval structures.
 * @return time elapsed between t1 and t2(in us). 
 */
long int get_time_interval(struct timeval t1,struct timeval t2){
	
	long int diff  = (t2.tv_sec+(t2.tv_usec/1.0e6))*1.0e6-(t1.tv_sec+(t1.tv_usec/1.0e6))*1.0e6;
	if (diff>0)
		return diff;
	else return 0;
		
}


/**
 * @name set_in_standby_mode - set consumer threads on standby mode until
 * 							   they receive a request. 
 * @param quit_mutex: a mutex variable used to terminate worker threads.
 * @return NULL
 */
void* set_in_standby_mode(void* quit_mutex){
	
	
	Request_descriptor *rd;
	int failure;
	struct timeval start_time, end_time;

	while(1){
		
		pthread_mutex_lock(&q_mutex);
		
		/*Wait while request queue is empty*/
		while (is_empty(request_queue)){			
			printf("\n(Info)Thread #%ld:Empty request queue. Wait...\n",(long int)(pthread_self()));
			pthread_cond_wait(&non_empty_queue,&q_mutex);
		
		}
		usleep(100);		
		if (pthread_mutex_trylock((pthread_mutex_t *)quit_mutex)==0){
			
			
			pthread_cond_broadcast(&non_empty_queue);
			pthread_mutex_unlock(&q_mutex);
			pthread_mutex_unlock((pthread_mutex_t *)quit_mutex);
			break;
				
		}else{
			
			printf("\n(Info)Thread #%ld:Up!Ready to serve client request.\n",(long int)(pthread_self()));
			
			rd =malloc(sizeof(Request_descriptor));
			*rd = *get_head(request_queue);
			
			dequeue(request_queue);

			if (!is_full(request_queue))
				pthread_cond_signal(&non_full_queue);

			gettimeofday(&start_time,NULL);
			failure = handle_request(rd);
			gettimeofday(&end_time,NULL);
			
			if(!failure){
			
				//compute waiting time
				pthread_mutex_lock(&waiting_time_mutex);
				total_waiting_time += get_time_interval(rd->start_time,start_time);
				pthread_mutex_unlock(&waiting_time_mutex);
				
				//compute service time
				pthread_mutex_lock(&service_time_mutex);
				total_service_time += get_time_interval(start_time,end_time);
				pthread_mutex_unlock(&service_time_mutex);			
				
				//increase completed requests
				pthread_mutex_lock(&comp_requests_mutex);
				++completed_requests;
				pthread_mutex_unlock(&comp_requests_mutex);
			}
			if(close(rd->fd)!=0)
				printf("\nERROR:closing file descriptor with fd:%d\n",rd->fd);
			free(rd);
			pthread_mutex_unlock(&q_mutex);
			
			
		}
		
	}
	
	return NULL;
	
}


/**
 * @name create_thread - used for creating consumer threads.
 * @param arg: a pointer to the mutex used to terminate program. 
 */ 
void create_threads(void *arg){
	
	int i;
	pthread_mutex_t *quit_mutex = arg;
	pthread_attr_t cons_attr;
	
	
	//initialize attr for consumer thread
	pthread_attr_init(&cons_attr);
	pthread_attr_setdetachstate(&cons_attr,PTHREAD_CREATE_JOINABLE);
	
	//create consumer threads to process the incoming requests
	for (i=0;i<MAX_CONSUMER_NUM;i++)
		pthread_create(&cons_threads[i],&cons_attr,set_in_standby_mode,(void *)quit_mutex);
	
	//destroy attribute
	pthread_attr_destroy(&cons_attr);
	
}


/**
 *@name print_stats - prints total statistics after consumer threads
 *					  have been terminated.
 */
void print_stats(){
	
	printf("\n----\tList of statistics\t----\n");
	printf("\nAverage waiting time:%.2lf us\n",total_waiting_time/completed_requests);
	printf("\nAverage service time:%.2lf us\n",total_service_time/completed_requests);
	printf("\nNumber of completed requests:%d\n",completed_requests);
	
}


/**
 *@name - signal_handler
 *@param:sig - signal number
 */
void signal_handler(int sig){
	
	
	Request_descriptor *dummy = malloc(sizeof(Request_descriptor));
	struct timeval* dummy_time = malloc(sizeof(struct timeval));
	dummy->fd = 5;
	dummy->start_time = *dummy_time;
	
	
	if (sig == SIGTSTP && !finished){
		
		
		finished = true;
		pthread_mutex_unlock(&stop_mutex);
		while(pthread_mutex_trylock(&q_mutex))
			; /*do nothing*/
		enqueue(request_queue,*dummy);
		pthread_cond_broadcast(&non_empty_queue);
		pthread_mutex_unlock(&q_mutex);
		
		/*Print total statistics*/
		print_stats();
		

	}
	free(dummy);
	free(dummy_time);
	

}


/**
 *@name print_current_time - converts a timeval structure
 *						     to printable date format.
 *@param:tv - a timeval structure
 */
void print_current_time(struct timeval* tv){
	
	char time_buffer[25];
	int millisec;
	struct tm* tm_info;
	
	 millisec = lrint((*tv).tv_usec/1000.0);
	 
	if (millisec>=1000) { 
		millisec -=1000;
		(*tv).tv_sec++;
	}
	
	tm_info = localtime(&((*tv).tv_sec));
	strftime(time_buffer, 26, "%Y/%m/%d %H:%M:%S", tm_info);
	printf("%s.%03d\n", time_buffer, millisec);
	
	
}


/**
 * @name main - The main routine.
 * @return 0 on success, 1 on error.
 */
int main() {

	int socket_fd,              // listen on this socket for new connections
	  new_fd;                 // use this socket to service a new connection
	socklen_t clen;
	struct sockaddr_in server_addr,  // my address information
					 client_addr;  // connector's address information
					 


	void *status;
	int rc,i;
	bool first_time=true;//used once to create worker threads.
	struct sigaction sa;
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

	//create request queue
	request_queue = create_queue(QUEUE_SIZE); 

	// create socket
	if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		ERROR("socket()");

	// Ignore the SIGPIPE signal in/order to not crash when a
	// client closes the connection unexpectedly.
	signal(SIGPIPE, SIG_IGN);

	//define the signal for termination
	if (sigaction(SIGTSTP,&sa,NULL)==-1)
	  exit(-1);

	// create socket adress of server (type, IP-adress and port number)
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);    // any local interface
	server_addr.sin_port = htons(MY_PORT);

	// bind socket to address
	if (bind(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
	ERROR("bind()");

	// start listening to socket for incoming connections
	listen(socket_fd, MAX_PENDING_CONNECTIONS);
	fprintf(stderr, "(Info) main: Listening for new connections on port %d ...\n", MY_PORT);
	clen = sizeof(client_addr);


	// Allocate memory for the database.
	if (!(db = (KISSDB *)malloc(sizeof(KISSDB)))) {
	fprintf(stderr, "(Error) main: Cannot allocate memory for the database.\n");
	return 1;
	}

	// Open the database.
	if (KISSDB_open(db, "mydb.db", KISSDB_OPEN_MODE_RWCREAT, HASH_SIZE, KEY_SIZE, VALUE_SIZE)) {
	fprintf(stderr, "(Error) main: Cannot open the database.\n");
	return 1;
	}


	//initialize mutexes
	pthread_mutex_init(&q_mutex,NULL);
	pthread_mutex_init(&stop_mutex,NULL);
	pthread_mutex_init(&comp_requests_mutex,NULL);
	pthread_mutex_init(&waiting_time_mutex,NULL);
	pthread_mutex_init(&service_time_mutex,NULL);
	pthread_mutex_init(&no_writer_mutex,NULL);
	pthread_mutex_init(&no_reader_mutex,NULL);
	pthread_mutex_init(&writer_count_mutex,NULL);
	pthread_mutex_init(&reader_count_mutex,NULL);

	//initialize condition variables
	pthread_cond_init(&non_empty_queue,NULL);
	pthread_cond_init(&non_full_queue,NULL);
	pthread_cond_init(&no_writer,NULL);
	pthread_cond_init(&no_reader,NULL);
	pthread_cond_init(&stop,NULL);


	//allocate memory for worker threads
	cons_threads = malloc(sizeof(pthread_t)*MAX_CONSUMER_NUM);
  
    pthread_mutex_lock(&stop_mutex);
	// main loop: wait for new connection/requests
	while (!finished) { 
		
	
		fprintf(stdout,"\n(Info) producer:waiting for new request.\n");
		
		// wait for incoming connection
		new_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &clen);
		if (new_fd < 0) {
			if (errno == EINTR){
				if (finished)
					continue;
				else
					ERROR("accept()");
			}
		}

		printf("\n--Info:new request arrived with fd:%d--\n",new_fd);

		 /*executed once to create worker threads*/
		if (first_time){
			create_threads(&stop_mutex);
			first_time=false;
		}

		// got connection
		struct timeval* start_time = malloc(sizeof(struct timeval));
		gettimeofday(start_time,NULL);
		fprintf(stdout, "\n(Info) producer: Got connection from '%s' on ", inet_ntoa(client_addr.sin_addr));
		print_current_time(start_time);

		//allocate memory for the file descriptor of the client connection
		Request_descriptor *rd = malloc(sizeof(Request_descriptor));
		if (!rd){
			
			fprintf(stderr, "\n(Error) main: Cannot allocate memory for the descriptor of the client.\n");
			return -1;
			
		}

		//Initialize request descriptor
		rd->fd = new_fd;
		rd->start_time = *start_time;

		
		pthread_mutex_lock(&q_mutex);
		
		//add request descriptor to queue
		enqueue(request_queue,*rd);
		
		//notify worker threads for a pending request
		if (!is_empty(request_queue))
			pthread_cond_signal(&non_empty_queue);
		
		pthread_mutex_unlock(&q_mutex);

		pthread_mutex_lock(&q_mutex);
		
		//check for full queue
		while (is_full(request_queue)){	
			printf("\n(Info) producer:request queue is full!!!.Wait...\n");
			pthread_cond_wait(&non_full_queue,&q_mutex);
		}
		
		pthread_mutex_unlock(&q_mutex);

		free(rd);
		free(start_time);
	
	}
	
	
	printf("\n(Info)producer:stop receiving requests.\n");

	/*main:waiting for consumer threads to terminate*/
	for ( i=0;i<MAX_CONSUMER_NUM;i++){
		rc = pthread_join(cons_threads[i],&status);
		if (rc){
			printf("\nERROR:return code from pthread_join() is %d\n",rc);
			exit(-1);
		}
		else
			printf("\n(Info)producer:join with WORKER thread successfully with return code:%d\n",rc);

	}

	free(status);
	free(request_queue);
	free(cons_threads);

	//destroy mutexes
	pthread_mutex_destroy(&q_mutex);
	pthread_mutex_destroy(&stop_mutex);
	pthread_mutex_destroy(&no_reader_mutex);
	pthread_mutex_destroy(&no_writer_mutex);
	pthread_mutex_destroy(&comp_requests_mutex);
	pthread_mutex_destroy(&waiting_time_mutex);
	pthread_mutex_destroy(&service_time_mutex);
	pthread_mutex_destroy(&writer_count_mutex);
	pthread_mutex_destroy(&reader_count_mutex);

	// Destroy the database.
	// Close the database.
	KISSDB_close(db);

	// Free memory.
	if (db)
	free(db);
	db = NULL;

	return 0; 
}
