CC = gcc
CFLAGS = -g -O2 -Wall -Wundef
OBJECTS = 

all: client server

client: client.c utils.o
	$(CC) $(CFLAGS) -o client client.c utils.o -lpthread 

server: server.c utils.o kissdb.o queue.o
	$(CC) $(CFLAGS) -o server server.c utils.o queue.o kissdb.o -lpthread -lm

%.o : %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o client server queue *.db
