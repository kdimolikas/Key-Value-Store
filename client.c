/* client.c

   Sample code of 
   Assignment L1: Simple multi-threaded key-value server
   for the course MYY601 Operating Systems, University of Ioannina 

   (c) S. Anastasiadis, G. Kappes 2016
	Modified by KD 2018
*/

#include<pthread.h>
#include "utils.h"

#define SERVER_PORT     6767
#define BUF_SIZE        2048
#define MAXHOSTNAMELEN  1024
#define MAX_STATION_ID   128
#define ITER_COUNT         1
#define GET_MODE           1
#define PUT_MODE           2
#define USER_MODE          3
#define MULTI_CLIENTS_MODE 4
#define MAX_CLIENTS_NUM   50

/*A struct for multiple clients*/
struct multi_clients{
	
	struct sockaddr_in *server_addr;
	int clients_index;
	
};

//a structure containing info about server - client 
struct multi_clients *clients_pt;

//client threads
pthread_t *clients;

//mutex variables for get_buffer and put_buffer variables
pthread_mutex_t get_buffer_mutex,put_buffer_mutex;

char get_buffer[BUF_SIZE],put_buffer[BUF_SIZE];


/**
 * @name print_usage - Prints usage information.
 */
void print_usage() {
  fprintf(stderr, "Usage: client [OPTION]...\n\n");
  fprintf(stderr, "Available Options:\n");
  fprintf(stderr, "-h:             Print this help message.\n");
  fprintf(stderr, "-a <address>:   Specify the server address or hostname.\n");
  fprintf(stderr, "-o <operation>: Send a single operation to the server.\n");
  fprintf(stderr, "                <operation>:\n");
  fprintf(stderr, "                PUT:key:value\n");
  fprintf(stderr, "                GET:key\n");
  fprintf(stderr, "-i <count>:     Specify the number of iterations.\n");
  fprintf(stderr, "-g:             Repeatedly send GET operations.\n");
  fprintf(stderr, "-p:             Repeatedly send PUT operations.\n");
}

/**
 * @name talk - Sends a message to the server and prints the response.
 * @server_addr: The server address.
 * @buffer: A buffer that contains a message for the server.
 *
 * @return
 */
void talk(const struct sockaddr_in server_addr, char *buffer) {
  char rcv_buffer[BUF_SIZE];
  int socket_fd, numbytes;
      
  // create socket
  if ((socket_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    ERROR("socket()");
  }

  // connect to the server.
  if (connect(socket_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1) {
    ERROR("connect()");
  }
  
  // send message.
  write_str_to_socket(socket_fd, buffer, strlen(buffer));
      
  // receive results.
  printf("Result: ");
  do {
    memset(rcv_buffer, 0, BUF_SIZE);
    numbytes = read_str_from_socket(socket_fd, rcv_buffer, BUF_SIZE);
    if (numbytes != 0)
      printf("%s", rcv_buffer); // print to stdout
  } while (numbytes > 0);
  printf("\n");
      
  // close the connection to the server.
  close(socket_fd);
}


/**
 *@name - send_multiple_requests defines the request send by clients (put if client_index even, get for odd client_index)
 *@param clients_pt: a pointer to a struct multi_clients.
 */
void* send_multiple_requests(void *clients_pt){
	
	
	int i,j;
	int offset = 5;
	int start = (((struct multi_clients*)clients_pt)->clients_index)*offset;
	struct sockaddr_in * serv_addr = malloc(sizeof(struct sockaddr_in));
	*serv_addr = *(((struct multi_clients*)clients_pt)->server_addr);
	
	if (start%2 == 0){
		pthread_mutex_lock(&put_buffer_mutex);
		
		for (i=start;i<start+offset;i++){
		
			memset(put_buffer, 0, BUF_SIZE);
			sprintf(put_buffer, "PUT:station.%d:%d", i, i);
			printf("Operation: %s\n", put_buffer);
			talk(*serv_addr, put_buffer);
		}
		
		pthread_mutex_unlock(&put_buffer_mutex);
	}else{
		pthread_mutex_lock(&get_buffer_mutex);
		usleep(200*1000);
		for (j=start;j<start+offset;j++){
			
			memset(get_buffer, 0, BUF_SIZE);
			sprintf(get_buffer, "GET:station.%d", j-offset);
			printf("Operation: %s\n", get_buffer);
			talk(*serv_addr, get_buffer);
		}
	
		pthread_mutex_unlock(&get_buffer_mutex);
	}
	free(serv_addr);
	return NULL;
	
}


/**
 *@name - create_clients
 *@param server_addr: server address.
 *@param clients_num: number of  clients.
 *@return 0 on Success.
 */
int create_clients(struct sockaddr_in *server_addr, int clients_num){
	
	int i;
	pthread_attr_t attr;
	
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
	
	for (i=0;i<clients_num;i++){
		
		(clients_pt+i)->server_addr = server_addr;
		(clients_pt+i)->clients_index = i;
		pthread_create(&clients[i],&attr,send_multiple_requests,(void*)&clients_pt[i]);
		
	}
	
	pthread_attr_destroy(&attr);
	return 0;
}


/**
 * @name main - The main routine.
 * @return 0 on Success.
 */
int main(int argc, char **argv) {
  char *host = NULL;
  char *request = NULL;
  int mode = 0;
  int option = 0;
  int count = ITER_COUNT;
  int clients_num = MAX_CLIENTS_NUM;
  char snd_buffer[BUF_SIZE];
  int station, value;
  struct sockaddr_in server_addr;
  struct hostent *host_info;
  int rc;
  int j;
  
  pthread_mutex_init(&get_buffer_mutex,NULL);
  pthread_mutex_init(&put_buffer_mutex,NULL);
  
  // Parse user parameters.
  while ((option = getopt(argc, argv,"i:hgpo:a:m:")) != -1) {
    switch (option) {
      case 'h':
        print_usage();
        exit(0);
      case 'a':
        host = optarg;
        break;
      case 'i':
        count = atoi(optarg);
		break;
      case 'g':
        if (mode) {
          fprintf(stderr, "You can only specify one of the following: -g, -p, -o\n");
          exit(EXIT_FAILURE);
        }
        mode = GET_MODE;
        break;
      case 'p': 
        if (mode) {
          fprintf(stderr, "You can only specify one of the following: -g, -p, -o\n");
          exit(EXIT_FAILURE);
        }
        mode = PUT_MODE;
        break;
      case 'o':
        if (mode) {
          fprintf(stderr, "You can only specify one of the following: -r, -w, -o\n");
          exit(EXIT_FAILURE);
        }
        mode = USER_MODE;
        request = optarg;
        break;
	  case 'm':
		if (mode){
			fprintf(stderr,"You can only specify one of the following: -g, -p, -o, -m\n");
			exit(EXIT_FAILURE);
		}
		mode = MULTI_CLIENTS_MODE;
		
		if (atoi(optarg)>MAX_CLIENTS_NUM){
			fprintf(stderr,"Max number of clients exceeds the limit.\n");
			printf("Available max number of clients:%d\n",MAX_CLIENTS_NUM);
			exit(EXIT_FAILURE);
		}
		clients_num = atoi(optarg);
		clients = malloc(sizeof(pthread_t)*clients_num);
		clients_pt = malloc(sizeof(struct multi_clients)*clients_num);
		break;
		
      default:
        print_usage();
        exit(EXIT_FAILURE);
    }
  }

  // Check parameters.
  if (!mode) {
    fprintf(stderr, "Error: One of -g, -p, -o is required.\n\n");
    print_usage();
    exit(0);
  }
  if (!host) {
    fprintf(stderr, "Error: -a <address> is required.\n\n");
    print_usage();
    exit(0);
  }
  
  // get the host (server) info
  if ((host_info = gethostbyname(host)) == NULL) { 
    ERROR("gethostbyname()"); 
  }
    
  // create socket adress of server (type, IP-adress and port number)
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr = *((struct in_addr*)host_info->h_addr);
  server_addr.sin_port = htons(SERVER_PORT);

  if (mode == USER_MODE) {
    memset(snd_buffer, 0, BUF_SIZE);
    strncpy(snd_buffer, request, strlen(request));
    printf("Operation: %s\n", snd_buffer);
    talk(server_addr, snd_buffer);
	
  }else if (mode == MULTI_CLIENTS_MODE){
	printf("(INFO) main:Ready to create clients\n");
	create_clients(&server_addr,clients_num);
	
	for (j=0;j<clients_num;j++){
		
		rc = pthread_join(clients[j],NULL);
		if (rc){
			printf("\nERROR:return code from pthread_join() is %d\n",rc);
			exit(-1);
		}
		else
			printf("\n(Info)main:join with client thread %d successfully with return code:%d\n",j,rc);

	}
	free(clients);
	free(clients_pt);
	  
  }else {
    while(--count>=0) {
      for (station = 0; station <= MAX_STATION_ID; station++) {
        memset(snd_buffer, 0, BUF_SIZE);
        if (mode == GET_MODE) {
          // Repeatedly GET.
          sprintf(snd_buffer, "GET:station.%d", station);
        } else if (mode == PUT_MODE) {
          // Repeatedly PUT.
          // create a random value.
          value = rand() % 65 + (-20);
          sprintf(snd_buffer, "PUT:station.%d:%d", station, value);
        }
        printf("Operation: %s\n", snd_buffer);
        talk(server_addr, snd_buffer);
      }
    }
  }
  pthread_mutex_destroy(&get_buffer_mutex);
  pthread_mutex_destroy(&put_buffer_mutex);
  return 0;
}

