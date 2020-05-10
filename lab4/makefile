# makefile for the STUN NAT-detector
#
# For Dartmouth COSC 60 Lab 4;
# By Shengsong Gao, May 2020.
#

CC = gcc
CFLAGS = -std=c11 -Wall
ALL = stun_symmetry stun_cone_level

.PHONY: test clean

all: $(ALL)

stun_symmetry: stun_symmetry.c
	@$(CC) $(CFLAGS) -o stun_symmetry stun_symmetry.c -lpthread

stun_cone_level: stun_cone_level.c
	@$(CC) $(CFLAGS) -o stun_cone_level stun_cone_level.c -lpthread

test_symmetry: stun_symmetry
	@./stun_symmetry

test_cone_level: stun_cone_level
	@./stun_cone_level

clean:
	@rm -f $(ALL)
