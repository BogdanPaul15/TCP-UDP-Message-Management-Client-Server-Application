CC = gcc
CFLAGS = -Wall -g -Werror -Wno-error=unused-variable -I include/
LIBS = -lm

all: server subscriber

common.o: lib/common.c
	$(CC) $(CFLAGS) -c lib/common.c -o common.o

client.o: lib/client.c
	$(CC) $(CFLAGS) -c lib/client.c -o client.o

server: server.c common.o client.o
	$(CC) $(CFLAGS) server.c common.o client.o $(LIBS) -o server

subscriber: subscriber.c common.o client.o
	$(CC) $(CFLAGS) subscriber.c common.o client.o $(LIBS) -o subscriber

.PHONY: clean

clean:
	rm -f server subscriber *.o