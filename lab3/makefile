# makefile for the multi-sender TCP local chatroom
# employing the custom-made Dual-Purpose Protocol.
#
# For Dartmouth COSC 60 Lab 2;
# By Shengsong Gao, 2020.
#

CC = gcc
CFLAGS = -std=c11 -Wall
OPAQUE_C = mrt.c Queue.c utilities.c
OPAQUE_H = mrt.h Queue.h utilities.h
ALL = sender receiver

.PHONY: test clean

all: $(ALL)

receiver_module: mrt_receiver.c $(OPAQUE_C) $(OPAQUE_H)
	$(CC) $(CFLAGS) -o receiver_module mrt_receiver.c $(OPAQUE_C) -lpthread

# remember that libraries must follow the objects and sources...
sender: sender.c mrt_sender.c $(OPAQUE_C) $(OPAQUE_H)
	$(CC) $(CFLAGS) -o sender sender.c mrt_sender.c $(OPAQUE_C) -lpthread
	
receiver: receiver.c mrt_receiver.c $(OPAQUE_C) $(OPAQUE_H)
	$(CC) $(CFLAGS) -o receiver receiver.c mrt_receiver.c $(OPAQUE_C) -lpthread

clean:
	rm -f $(ALL)
