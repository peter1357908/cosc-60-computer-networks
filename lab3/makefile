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
ALL = sender receiver number_writer

.PHONY: test clean

all: $(ALL)

# remember that libraries must follow the objects and sources...
sender: sender.c mrt_sender.c mrt_sender.h $(OPAQUE_C) $(OPAQUE_H)
	@$(CC) $(CFLAGS) -o sender sender.c mrt_sender.c $(OPAQUE_C) -lpthread
	
receiver: receiver.c mrt_receiver.c mrt_receiver.h $(OPAQUE_C) $(OPAQUE_H)
	@$(CC) $(CFLAGS) -o receiver receiver.c mrt_receiver.c $(OPAQUE_C) -lpthread

number_writer: number_writer.c
	@$(CC) $(CFLAGS) -o number_writer number_writer.c

test_sender1: sender
	@./sender 4242 1000

test_sender2: sender
	@./sender 9999 20

test_sender_comma: sender number_writer
	@./number_writer 2000 1 | ./sender 4343 1000

test_sender_newline: sender number_writer
	@./number_writer 2000 0 | ./sender 4444 20

test_receiver1: receiver number_writer
	@./receiver 1 > output
	@./number_writer 2000 1 > supposed_output
	@diff output supposed_output

test_receiver2: receiver
	@./receiver 2

clean:
	@rm -f $(ALL)
