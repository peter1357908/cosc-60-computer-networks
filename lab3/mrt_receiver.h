/* Header file for `_mrt_receiver_h.c`
 * Receiver functions for the Mini Reliable Transport module.
 *
 * For Dartmouth COSC 60 Lab 3;
 * By Shengsong Gao, April 2020.
 */

#ifndef _mrt_receiver_h
#define _mrt_receiver_h

#define MAX_WINDOW_SIZE (MAX_MRT_PAYLOAD_SIZE * 5)

int mrt_close();

#endif // _mrt_receiver_h
