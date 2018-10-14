# Key - Value Store

Introducing concurrent service of multiple client requests related to the data storage or retrieval in a simple Key-Value (KV) store exploiting a multithreaded client-server communication.

The server is responsible for maintaining key-value pairs based on the open-source key-value store [KISSDB][1]. The server adopts the producer-consumer structure, in which one thread (producer) waits for client requests and a predefined number of other threads (consumers) execute the client requests. The server also preserves statistical info about the number of completed requests, the average service time which is the time required to retrieve data from store and the average waiting time which is the time the request remains in queue until the search starts.

The client(s) can request the storage or retrieval of key-value pairs to or from the store. The user can define the number of the threads that operate as clients.

As for the manipulation of the queue of pending requests, we should mention that queue is accessible from all the threads of the server, which entails the implementation of a synchronization control. For this reason we introduced various mutexes which ensure that simultaneous updates in the store cannot occur. We also used two condition variables in order to facilitate the communication between producer and consumers when the queue is either empty or full.  

## Execution
The following commands are included in our application:
* make all: compile all source files.
* make clean: remove all files created with the command \<make all>.
* ./server: start the server
* ./client -a <server_address> -m <number_of_clients> : start multiple clients which send a predefined number of requests to the server.
* ./client -a <server_address> -i 1 -p: put some data to KV store (for 1 client). 
* ./client -a <server_address> -i 1 -g: get all data from store (for 1 client).
* ./client -a <server_address> -o GET:station.125: get the value of station with key 125.
* ./client -a <server_address> -o PUT:station.125:13: set the value 13 for station with key 125.
* CTRL-Z: terminate server and print the statistical info.


### The code of the server and the client were provided by S. Anastasiadis and G. Kappes. My main cotribution is that of modifying the existing code in order to make possible that server will response to multiple client requests ensuring at the same time the integrity of store's data. 

[1]: https://github.com/adamierymenko/kissdb  