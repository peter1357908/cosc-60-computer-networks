/* Header file for `mrt_sender.c`
 * Sender functions for the Mini Reliable Transport module.
 *
 * For Dartmouth COSC 60 Lab 3;
 * By Shengsong Gao, April 2020.
 */

#ifndef _mrt_sender_h
#define _mrt_sender_h

unsigned short *mrt_connect();

/* returns the number of bytes successfully sent (acknowledged).
 * will block until the corresponding final ADAT is processed.
 *
 * Returns -1 if the call is spurious (connection not accepted yet,
 * mrt_open() not even called yet, etc.)
 */
int mrt_send(void *buffer, int len);

/* will wait until final ADAT is received to send a RCLS
 * (unless signaled to close by timeout). Blocking.
 */
void mrt_disconnect();

#endif // _mrt_sender_h
