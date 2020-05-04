/* Header file for `mrt_sender.c`
 * Sender functions for the Mini Reliable Transport module.
 *
 * For Dartmouth COSC 60 Lab 3;
 * By Shengsong Gao, April 2020.
 */

#ifndef _mrt_sender_h
#define _mrt_sender_h

/* returns the connection ID (int; positive)
 * returns -1 upon any error
 * will block until the connection is established
 *
 * the `s_addr` should have the same format as
 * `(struct sockaddr_in).sin_addr.s_addr`
 * `s_addr` will be put inside `htonl()` before use
 */
int mrt_connect(unsigned short port_number, unsigned long s_addr);

/* Returns 1 if all bytes are successfully sent (acknowledged).
 * Will block until the corresponding final ADAT is processed (large
 * enough data will be split into multiple fragments).
 *
 * Returns 0 if the call ends prematurely due to the connection
 * being dropped 
 * TODO: any efficient way to tell the acknowledged bytes in this case?
 *
 * Returns -1 if the call is spurious (connection not accepted yet,
 * mrt_open() not even called yet, etc.)
 *
 * Does not support getting called multiple times concurrently
 * for the same connection (undefined behavior if attempted).
 */
int mrt_send(int id, char *buffer, int len);

/* will wait until final ADAT is received to send a RCLS
 * (unless signaled to close by timeout). Blocking.
 */
void mrt_disconnect(int id);

#endif // _mrt_sender_h
