/* Receiver functions for the Mini Reliable Transport module.
 *	
 * For Dartmouth COSC 60 Lab 3;
 * By Shengsong Gao, April 2020.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // exit(), malloc(), free()
#include <unistd.h> // close(), sleep()
#include <sys/socket.h>
#include <arpa/inet.h> // htons()
#include <pthread.h>

#include "mrt.h"
#include "mrt_receiver.h"
#include "Queue.h"
#include "utilities.h" // hash()

#define CHECKER_PERIOD          EXPECTED_RTT * 4
#define TIMEOUT_THRESHOLD       CHECKER_PERIOD * 3
#define ACCEPT1_PERIOD          EXPECTED_RTT * 2 // connection request
#define RECEIVE1_PERIOD         EXPECTED_RTT * 2 // sender buffer

/****** declarations ******/
typedef struct sender {
  struct sockaddr_in addr;
  char buffer[RECEIVER_MAX_WINDOW_SIZE];
  int bytes_unread;
  int next_frag;
  int inactive_time;
  pthread_t checker_thread; // checks for inactivity
} sender_t;

void *main_handler(void *_null);
void *checker(void *sender_vp);
int sender_matcher(void *sender_vp, void *id_vp);
void probe_for_one(void *id_vp, void *target_id_vpp);
void build_acon(int initial_frag);
void build_adat(int received_frag, int curr_window_size);
void build_acls();

/****** global variables ******/
unsigned int addr_len = (unsigned int) sizeof(struct sockaddr_in);
int initial_window_size = RECEIVER_MAX_WINDOW_SIZE;
int rece_sockfd;

int should_close = 0;
pthread_mutex_t close_lock = PTHREAD_MUTEX_INITIALIZER;

q_t *pending_senders_q;
q_t *connected_senders_q;
pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_t main_thread;
char incoming_buffer[MAX_UDP_PAYLOAD_LENGTH + 1]; // +1 for NULL-termination for hash()
char outgoing_buffer[MRT_HEADER_LENGTH + 1]; // +1 for NULL-termination for hash()

/****** functions ******/

/* will create the main thread that handles all incoming transmissions
 * returns -1 upon any error and 0 upon success.
 */
int mrt_open(unsigned int port_number) {
  /****** initializing socket and receiver address ******/
  rece_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (rece_sockfd < 0)
  {
    perror("rece_sockfd = socket() error\n");
    return -1;
  }

  struct sockaddr_in rece_addr = {0};
  rece_addr.sin_family = AF_INET;
  rece_addr.sin_port = htons(port_number);
  rece_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  /****** binding the socket ******/
  if (bind(rece_sockfd, (struct sockaddr *)&rece_addr, addr_len) < 0)
  {
    perror("bind(rece_sockfd) error\n");
    return -1;
  }

  /****** initiating the main handler ******/
  pthread_mutex_lock(&q_lock);
    pending_senders_q = make_q();
    connected_senders_q = make_q();
    if (pending_senders_q == NULL || connected_senders_q == NULL) {
      perror("make_q() returned NULL\n");
      return -1;
    }
  pthread_mutex_unlock(&q_lock);

  if (pthread_create(&main_thread, NULL, main_handler, NULL) != 0) {
    perror("pthread_create(main_thread) error\n");
    return -1;
  }
  
  return 0;
}

/* accepts a connection request and returns a pointer to a copy of
 * its ID struct (currently reusing `sockaddr_in`). 
 * If no requests exist yet, will block and wait until one shows up,
 * and then accept it.
 * the sender is responsible for freeing the ID struct.
 */
struct sockaddr_in *mrt_accept1() {
  sender_t *curr_sender = NULL;
  while (1) {
    pthread_mutex_lock(&q_lock);
      curr_sender = deq_q(pending_senders_q);
      if (curr_sender == NULL) {
    pthread_mutex_unlock(&q_lock);
        sleep(ACCEPT1_PERIOD); 
      } else {
    pthread_mutex_unlock(&q_lock);
        break;
      }
  }
  /* note that the two queues are created in one atomic action
   * so breaking out of the above while loop mean that both queues
   * are already initialized correctly... and so are other variables
   * used below; they are initialized before the two queues.
   */
  pthread_mutex_lock(&q_lock);
    enq_q(connected_senders_q, curr_sender);

    /* TODO: be semantically correct and use a different buffer
     * than the main outgoing_buffer (currently this should still
     * work, though)
     */
    build_acon(curr_sender->next_frag - 1);
    sendto(rece_sockfd, outgoing_buffer, MRT_HEADER_LENGTH,  
      0, (const struct sockaddr *)(&(curr_sender->addr)), 
      addr_len);

    // as soon the ACON is sent, start the timeout checker thread
    // TODO: what if pthread_create() fails? FATAL? Retry-worthy?
    pthread_create(&(curr_sender->checker_thread), NULL, checker, curr_sender);
    
    // make a copy of the ID struct
    struct sockaddr_in *id_p = malloc(addr_len);
    memmove(id_p, &(curr_sender->addr), addr_len);

  pthread_mutex_unlock(&q_lock);
  
  // TODO: dangerously asynchronous? Does it matter?
  return id_p;
}

/* Will accepted all the pending connections requests
 * and return a queue of their IDs (struct sockaddr_in, as of now).
 * Does not wait for a request to show up; will return an 
 * empty queue if there are no pending requests.
 *
 * The caller is responsible for freeing the (q_t *) as well
 * all the IDs.
 */
q_t *mrt_accept_all() {
  q_t *accepted_q = make_q();
  sender_t *curr_sender;
  struct sockaddr_in *id_p = NULL;

  while (1) {
    pthread_mutex_lock(&q_lock);
      curr_sender = peek_q(pending_senders_q);
      if (curr_sender == NULL) {
    pthread_mutex_unlock(&q_lock);
      break;
    } else {
    pthread_mutex_unlock(&q_lock);
      id_p = mrt_accept1();
      enq_q(accepted_q, id_p);
    }
  }

  return accepted_q;
}

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
int mrt_receive1(struct sockaddr_in *id_p, void *buffer, int len) {
  sender_t *curr_sender = NULL;
  pthread_mutex_lock(&q_lock);
    curr_sender = get_item_q(connected_senders_q, sender_matcher, id_p);
    if (curr_sender == NULL) { 
  pthread_mutex_unlock(&q_lock);
      return -1; 
    }
  pthread_mutex_unlock(&q_lock);
  
  while(1) {
    pthread_mutex_lock(&q_lock);
      // get the sender again to ensure the connection is still valid
      curr_sender = get_item_q(connected_senders_q, sender_matcher, id_p);
      if (curr_sender == NULL) {
        // the sender is NULL now... after not being NULL once...
    pthread_mutex_unlock(&q_lock);
        return 0;    
      }

      // the connection remains; now either sleep or perform the copy
      if (curr_sender->bytes_unread <= 0) {
    pthread_mutex_unlock(&q_lock);
        sleep(RECEIVE1_PERIOD);
      } else {
        int bytes_unread = curr_sender->bytes_unread;
        int bytes_read = (len < bytes_unread) ? len : bytes_unread;
        memmove(buffer, curr_sender->buffer, bytes_read);
        // TODO: simplify the following if-else?
        if (bytes_read < bytes_unread) {
          // if there are bytes remaining:
          int bytes_remaining = bytes_unread - bytes_read;
          // note that this relies on memmove() to handle the overlapping
          memmove(curr_sender->buffer, (curr_sender->buffer) + bytes_read, bytes_remaining);
          curr_sender->bytes_unread = bytes_remaining;
    pthread_mutex_unlock(&q_lock);
          return bytes_read;
        } else {
          // if all bytes were read:
          curr_sender->bytes_unread = 0;
    pthread_mutex_unlock(&q_lock);
          return bytes_read;
        }
      }
  }
}

/* Returns the first connection that has some unread bytes
 * found in input queue.
 *
 * Expects a queue of IDs; returns either a pointer to a copy of a ID
 * or NULL (no satisfying ID found; does not wait for one).
 *
 * The user is responsible for freeing the returned copy.
 */
struct sockaddr_in *mrt_probe(q_t *probe_q) {
  struct sockaddr_in *target_id_p = NULL;
  iterate_q(probe_q, probe_for_one, &target_id_p);
  return target_id_p;
}

/* the actual logic is handled in main_handler()...
 */
void mrt_close() {
  pthread_mutex_lock(&close_lock);
    should_close = 1;
  pthread_mutex_unlock(&close_lock);
}

/****** thread functions (unavailable to module users) ******/

/* The main handler; all incoming transmissions will be validated
 * and handled here.
 */
void *main_handler(void *_null) {
  int bytes_received = 0;
  sender_t *curr_sender = NULL;
  struct sockaddr_in addr_holder = {0}; // to hold the addr of incoming transmission
  unsigned long hash_holder = 0;
  unsigned int addr_len_holder = 0;
  int type_holder = 0, frag_holder = 0;

  // the main loop; processes all the incoming transmissions
  while (1) {
    bytes_received = recvfrom(rece_sockfd, incoming_buffer,
      MAX_UDP_PAYLOAD_LENGTH, 0, (struct sockaddr *)(&addr_holder),
      &addr_len_holder);

    // before processing, check if close is flagged
    pthread_mutex_lock(&close_lock);
      if (should_close == 1) {
        printf("Breaking from main loop because should_close\n");
    pthread_mutex_unlock(&close_lock);
        break;
      }
    pthread_mutex_unlock(&close_lock);

    // NULL-terminate the transmission to enable hash()
    incoming_buffer[bytes_received] = '\0';

    // first validate the transmission with checksum
    memmove(&hash_holder, incoming_buffer, MRT_HASH_LENGTH);
    if (hash(incoming_buffer + MRT_HASH_LENGTH) != hash_holder) {
      printf("transmission discarded due to checksum mismatch\n");
      continue;
    }

    // then check the transmission type and act accordingly
    memmove(&type_holder, incoming_buffer + MRT_TYPE_LOCATION, MRT_TYPE_LENGTH);
    memmove(&frag_holder, incoming_buffer + MRT_FRAGMENT_LOCATION, MRT_FRAGMENT_LENGTH);
    switch (type_holder) {
      
      case MRT_RCON :
        // if the sender is not queued...
        pthread_mutex_lock(&q_lock);
          curr_sender = get_item_q(pending_senders_q, sender_matcher, &addr_holder);
          if (curr_sender == NULL) {
            // AND not connected, it must be a new sender... queue it.
            curr_sender = get_item_q(connected_senders_q, sender_matcher, &addr_holder);
            if (curr_sender == NULL) {
                curr_sender = malloc(sizeof(sender_t));
                curr_sender->bytes_unread = 0;
                curr_sender->next_frag = frag_holder + 1;
                curr_sender->inactive_time = 0;
                memmove(&(curr_sender->addr), &addr_holder, addr_len);
                enq_q(pending_senders_q, curr_sender);
            }
            // if it is already connected, send a (duplicate) ACON
            else {
              build_acon(frag_holder);
              sendto(rece_sockfd, outgoing_buffer, MRT_HEADER_LENGTH,  
                0, (const struct sockaddr *)(&addr_holder), 
                addr_len);
            }
          }
          /* else the sender is queued, and must not be already connected
          * do nothing (drop the packet)
          * Assumption here: all RCONs from one sender propose the same
          * initial fragment number
          */
        pthread_mutex_unlock(&q_lock);
        break;

      case MRT_DATA :
        pthread_mutex_lock(&q_lock);
          curr_sender = get_item_q(connected_senders_q, sender_matcher, &addr_holder);
          if (curr_sender != NULL) {
            /* buffer the transmitted payload if there is enough free space
             * AND it is not out of order;
             */
            int curr_window_size = RECEIVER_MAX_WINDOW_SIZE - curr_sender->bytes_unread;
            int payload_size = bytes_received - MRT_HEADER_LENGTH;
            if (curr_window_size >= payload_size && curr_sender->next_frag == frag_holder) {
              char *next_free_slot = (curr_sender->buffer) + curr_sender->bytes_unread;
              memmove(next_free_slot, incoming_buffer + MRT_PAYLOAD_LOCATION, payload_size);
              curr_sender->bytes_unread += payload_size;
              curr_sender->next_frag += 1;
              curr_window_size -= payload_size;
            }
            // else the packet must be dropped (out of order / buffer space)

            /* either way, sender just proved that he's still connected,
             * so reset the inactivity counter and replies with ADAT
             */
            curr_sender->inactive_time = 0;
            build_adat(curr_sender->next_frag - 1, curr_window_size);
            sendto(rece_sockfd, outgoing_buffer, MRT_HEADER_LENGTH,  
                    0, (const struct sockaddr *)(&addr_holder), 
                    addr_len);
          }
          // else the sender is sending data without being connected
          // do nothing (drop the packet)
        pthread_mutex_unlock(&q_lock);
        break;

      case MRT_RCLS :
        pthread_mutex_lock(&q_lock);
          curr_sender = get_item_q(connected_senders_q, sender_matcher, &addr_holder);
          /* note that RCLS is only sent upon receiving the final ADAT,
           * so there is no need to check/use the fragment number here.
           */
          if (curr_sender != NULL) {
            // trick the checker into doing clean-up
            curr_sender->inactive_time = TIMEOUT_THRESHOLD;
            // then be polite and do an ACLS
            build_acls();
            sendto(rece_sockfd, outgoing_buffer, 
              (MRT_HASH_LENGTH + MRT_TYPE_LENGTH), 0, 
              (const struct sockaddr *)(&addr_holder), addr_len);
          } else {
            /* else the sender is trying to disconnect without being connected;
            * in that case, just try to remove it from the queue...
            */
            free(pop_item_q(pending_senders_q, sender_matcher, &addr_holder));
          }
        pthread_mutex_unlock(&q_lock);
        break;

      default :
        // ACON, ADAT, ACLS, UNKN
        printf("transmission had good checksum but bad type = %d\n HOW????\n", type_holder);
        continue;
    }
  }
  /* No longer accepting new connections...
   * TODO: there must be a better way than pthread_cancel()...
   */
  pthread_mutex_lock(&q_lock);
    delete_q(pending_senders_q, free);
    while((curr_sender = (sender_t *)deq_q(connected_senders_q)) != NULL) {
      pthread_cancel(curr_sender->checker_thread);
      free(curr_sender);
    }
    delete_q(connected_senders_q, free); // could use free(q) directly
  pthread_mutex_unlock(&q_lock);

  close(rece_sockfd);
  return NULL;
}

/* checker: runs in a new thread for each sender as soon as its first
 * DATA is received; whether the transmission ends successfully or 
 * as a result of a timeout, this function garbage-collects the sender
 * before terminating
 */
void *checker(void *sender_vp) {
  sender_t *sender_p = (sender_t *)sender_vp;

  while (1) {
    pthread_mutex_lock(&q_lock);
      sender_p->inactive_time += CHECKER_PERIOD;
      // if it would sleep past the threshold, go BOOM
      if (sender_p->inactive_time > TIMEOUT_THRESHOLD) {
    pthread_mutex_unlock(&q_lock);
        break;
      }
    pthread_mutex_unlock(&q_lock);
    sleep(CHECKER_PERIOD);
  }
  // garbage collection
  pthread_mutex_lock(&q_lock);
    pop_item_q(connected_senders_q, sender_matcher, &(sender_p->addr));
  pthread_mutex_unlock(&q_lock);
  free(sender_p);

  return NULL;
}


/****** helper functions (unavailable to module users) ******/

/* returns 1 if the sender's addr matches
 * the input addr (byte by byte with memcmp()); returns 0 otherwise
 *
 * designed to be used as a Queue module callback function
 */
int sender_matcher(void *sender_vp, void *id_vp) {
  sender_t *sender_p = (sender_t *)sender_vp;
  struct sockaddr_in *id_p = (struct sockaddr_in *)id_vp;

  if (memcmp(&(sender_p->addr), id_p, addr_len) == 0) {
    return 1;
  }
  return 0;
}

/* callback for mrt_probe, expects to take in a pointer to an
 * NULL (struct sockaddr_in *), which would be set to a valid pointer
 * if a match is found.
 */
void probe_for_one(void *id_vp, void *target_id_vpp) {
  struct sockaddr_in **target_id_pp = (struct sockaddr_in  **)target_id_vpp;
  struct sockaddr_in  *target_id_p  = *target_id_pp;
  if (target_id_p != NULL) {
    struct sockaddr_in *id_p = (struct sockaddr_in  *)id_vp;
    sender_t *curr_sender;
    pthread_mutex_lock(&q_lock);
      curr_sender = get_item_q(connected_senders_q, sender_matcher, id_p);
      if (curr_sender != NULL && curr_sender->bytes_unread > 0) {
        target_id_p = malloc(addr_len);
        memmove(target_id_p, id_p, addr_len);
      }
      // otherwise a mismatch; do nothing.
    pthread_mutex_unlock(&q_lock);
  }
  // otherwise the target is already found; do nothing.
}

// the build_x() functions assume that memmove() always succeeds
void build_acon(int initial_frag) {
  memmove(outgoing_buffer + MRT_TYPE_LOCATION, &acon_type, MRT_TYPE_LENGTH);
  memmove(outgoing_buffer + MRT_FRAGMENT_LOCATION, &initial_frag, MRT_FRAGMENT_LENGTH);
  /* note that senders ignore ACONs beyond the first one, so the advertised
   * window size here can stay the same as the initial window size
   */
  memmove(outgoing_buffer + MRT_WINDOWSIZE_LOCATION, &initial_window_size, MRT_WINDOWSIZE_LENGTH);
  
  outgoing_buffer[MRT_HEADER_LENGTH] = '\0';
  unsigned long hash_holder = hash(outgoing_buffer + MRT_HASH_LENGTH);
  memmove(outgoing_buffer, &hash_holder, MRT_HASH_LENGTH);
}

void build_adat(int received_frag, int curr_window_size) {
  memmove(outgoing_buffer + MRT_TYPE_LOCATION, &adat_type, MRT_TYPE_LENGTH);
  memmove(outgoing_buffer + MRT_FRAGMENT_LOCATION, &received_frag, MRT_FRAGMENT_LENGTH);
  memmove(outgoing_buffer + MRT_WINDOWSIZE_LOCATION, &curr_window_size, MRT_WINDOWSIZE_LENGTH);
  
  outgoing_buffer[MRT_HEADER_LENGTH] = '\0';
  unsigned long hash_holder = hash(outgoing_buffer + MRT_HASH_LENGTH);
  memmove(outgoing_buffer, &hash_holder, MRT_HASH_LENGTH);
}

void build_acls() {
  memmove(outgoing_buffer + MRT_TYPE_LOCATION, &acls_type, MRT_TYPE_LENGTH);
  
  outgoing_buffer[MRT_HASH_LENGTH + MRT_TYPE_LENGTH] = '\0';
  unsigned long hash_holder = hash(outgoing_buffer + MRT_HASH_LENGTH);
  memmove(outgoing_buffer, &hash_holder, MRT_HASH_LENGTH);
}

