CC = gcc
CFLAGS = -std=c11 -Wall

.PHONY: test clean

all: client

# remember that libraries must follow the objects and sources...
client: client.c dpp.c dpp.h
	$(CC) $(CFLAGS) -o client client.c dpp.c -lpthread

test: clean all
	./client 4242 Peter

clean:
	rm -f client