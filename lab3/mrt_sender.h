/* Header file for `mrt_sender.c`
 * Sender functions for the Mini Reliable Transport module.
 *
 * For Dartmouth COSC 60 Lab 3;
 * By Shengsong Gao, April 2020.
 */

#ifndef _mrt_sender_h
#define _mrt_sender_h

// types of transmissions
#define MRT_UNKN  	0
#define MRT_RCON    1
#define MRT_ACON    2
#define MRT_DATA    3
#define MRT_ADAT    4
#define MRT_RCLS    5
#define MRT_ACLS    6

#define MRT_ERROR   -78
#define MRT_SUCCESS -42

#endif // _mrt_sender_h
