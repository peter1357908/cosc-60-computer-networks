# makefile for the multi-client TCP local chatroom
# employing the custom-made Dual-Purpose Protocol.
#
# For Dartmouth COSC 60 Lab 2;
# By Shengsong Gao, 2020.
#

CC = gcc
CFLAGS = -std=c11 -Wall

.PHONY: test clean

all: client server

# remember that libraries must follow the objects and sources...
client: client.c dpp.c dpp.h
	$(CC) $(CFLAGS) -o client client.c dpp.c -lpthread
	
server: server.c dpp.c dpp.h Queue.c Queue.h
	$(CC) $(CFLAGS) -o server server.c dpp.c Queue.c -lpthread

test_client: clean client
	./client 4242 Peter
	
test_server: clean server
	./server 4242

clean:
	rm -f client server