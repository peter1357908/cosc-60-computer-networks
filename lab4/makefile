# makefile for the STUN NAT-detector
#
# For Dartmouth COSC 60 Lab 4;
# By Shengsong Gao, May 2020.
#

CC = gcc
CFLAGS = -std=c11 -Wall
ALL = stun_client

.PHONY: test clean

all: $(ALL)

stun_client: stun_client.c
	@$(CC) $(CFLAGS) -o stun_client stun_client.c -lpthread

test: stun_client
	@./stun_client

clean:
	@rm -f $(ALL)
