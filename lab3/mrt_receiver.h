/* Header file for `_mrt_receiver_h.c`
 * Receiver functions for the Mini Reliable Transport module.
 *
 * For Dartmouth COSC 60 Lab 3;
 * By Shengsong Gao, April 2020.
 */

#ifndef _mrt_receiver_h
#define _mrt_receiver_h

#include "Queue.h"  // q_t

#define RECEIVER_MAX_WINDOW_SIZE (MAX_MRT_PAYLOAD_LENGTH * 5)

// will create the main thread that handles all incoming transmissions
int mrt_open(unsigned int port_number);

/* accepts a connection request and returns a pointer to a copy of
 * its ID struct (currently reusing `sockaddr_in`). 
 * If no requests exist yet, will block and wait until one shows up,
 * and then accept it.
 * the sender is responsible for freeing the ID struct.
 */
struct sockaddr_in *mrt_accept1();

/* Will accepted all the pending connections requests
 * and return a queue of their IDs (struct sockaddr_in, as of now).
 * Does not wait for a request to show up; will return an 
 * empty queue if there are no pending requests.
 *
 * The caller is responsible for freeing the (q_t *) as well
 * all the IDs.
 */
q_t *mrt_accept_all();

/* Moves to `buffer` at least 1 byte of data and as much as specified
 * in `len` as long as the connection remains open. Will block and wait
 * until there is data.
 *
 * Returns the number of bytes written.
 * Returns 0 if the connection dies while waiting for data
 * Returns -1 if the call is spurious (connection not accepted yet,
 * mrt_open() not even called yet, etc.)
 */
 // TODO: use CVAR instead of spurious sleep wakeups
int mrt_receive1(struct sockaddr_in *id_p, void *buffer, int len);

/* Returns the first connection that has some unread bytes
 * found in input queue.
 *
 * Expects a queue of IDs; returns either a pointer to a copy of a ID
 * or NULL (no satisfying ID found; does not wait for one).
 *
 * The user is responsible for freeing the returned copy.
 */
struct sockaddr_in *mrt_probe(q_t *probe_q);

/* the actual logic is handled in main_handler()...
 */
void mrt_close();

#endif // _mrt_receiver_h
