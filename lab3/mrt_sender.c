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
#include "mrt_sender.h"
#include "utilities.h" // hash()

#define CHECKER_PERIOD          2000                  // milliseconds
#define TIMEOUT_THRESHOLD       (CHECKER_PERIOD * 5)

/****** declarations ******/

void *main_handler(void *_null);
void *checker(void *sender_vp);
void build_acon(int initial_frag);
void build_adat(char *buffer, int received_frag, int curr_window_size);
void build_acls();

/****** global variables ******/
unsigned int addr_len = (unsigned int) sizeof(struct sockaddr_in);
int send_sockfd;
struct sockaddr_in rece_addr = {0};

int window_size = 0;
pthread_mutex_t winsize_lock = PTHREAD_MUTEX_INITIALIZER;

int inactive_time = 0;
pthread_mutex_t timeout_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_t handler_thread, checker_thread, 
char incoming_buffer[MRT_HEADER_LENGTH + 1]; // +1 for NULL-termination for hash()
char outgoing_buffer[MAX_UDP_PAYLOAD_LENGTH + 1]; // +1 for NULL-termination for hash()

/****** functions ******/

int mrt_connect() {
  /****** initializing socket and receiver address ******/
  send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (send_sockfd < 0)
  {
    perror("send_sockfd = socket() error\n");
    return MRT_ERROR;
  }

  rece_addr.sin_family = AF_INET;
  rece_addr.sin_port = htons(PORT_NUMBER);
  rece_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  /****** just keep trying to connect to server... ******/

  while(1) {

  }

  if (pthread_create(&main_thread, NULL, main_handler, NULL) != 0) {
    perror("pthread_create(main_thread) error\n");
    return MRT_ERROR;
  }
  
  return MRT_SUCCESS;
}


/****** thread functions (unavailable to module users) ******/

/* The main handler; all incoming transmissions will be validated
 * and handled here.
 */
void *main_handler(void *_null) {
  int bytes_received = 0;
  struct sockaddr_in addr_holder = {0}; // to hold the addr of incoming transmission
  unsigned short port_holder = 0;
  unsigned long hash_holder = 0;
  int type_holder = 0, frag_holder = 0;

  // the main loop; processes all the incoming transmissions
  while (1) {
    bytes_received = recvfrom(send_sockfd, incoming_buffer,
      MAX_MRT_PAYLOAD_LENGTH, 0, (struct sockaddr *)(&addr_holder),
      &addr_len);

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
    port_holder = addr_holder.sin_port;
    switch (type_holder) {
      
      case MRT_RCON :
        // if the sender is not queued...
        pthread_mutex_lock(&q_lock);
          curr_sender = get_item_q(pending_senders_q, sender_matcher, &port_holder);
          if (curr_sender == NULL) {
            // AND not connected, it must be a new sender... queue it.
            curr_sender = get_item_q(connected_senders_q, sender_matcher, &port_holder);
            if (curr_sender == NULL) {
                curr_sender = malloc(sizeof(sender_t));
                curr_sender->bytes_unread = 0;
                curr_sender->next_frag = frag_holder + 1;
                curr_sender->inactive_time = 0;
                curr_sender->checker_thread = NULL;
                memmove(&(curr_sender->addr), &addr_holder, addr_len);
                enq_q(pending_senders_q, curr_sender);
            }
            // if it is already connected, send a (duplicate) ACON
            else {
              build_acon(frag_holder);
              sendto(send_sockfd, outgoing_buffer, MRT_HEADER_LENGTH,  
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
          curr_sender = get_item_q(connected_senders_q, sender_matcher, &port_holder);
          if (curr_sender != NULL) {
            /* buffer the transmitted payload if there is enough free space
             * AND it is not out of order;
             */
            int curr_window_size = MAX_WINDOW_SIZE - curr_sender->bytes_unread;
            int payload_size = bytes_received - MRT_HEADER_LENGTH;
            if (curr_window_size >= payload_size && curr_sender->next_frag == frag_holder) {
              char *next_free_slot = (curr_sender->buffer) + curr_sender->bytes_unread;
              memmove(next_free_slot, incoming_buffer + MRT_PAYLOAD_LOCATION, payload_size);
              curr_sender->bytes_unread += payload_size;
              curr_sender->next_frag += 1;
              curr_window_size -= payload_size;
              build_adat(outgoing_buffer, frag_holder, curr_window_size);
              sendto(send_sockfd, outgoing_buffer, MRT_HEADER_LENGTH,  
                0, (const struct sockaddr *)(&addr_holder), 
                addr_len);
            }
            // else the packet must be dropped (out of order / buffer space)

            /* either way, sender just proved that he's still connected,
             * so reset the inactivity counter
             */
            curr_sender->inactive_time = 0;
          }
          // else the sender is sending data without being connected
          // do nothing (drop the packet)
        pthread_mutex_unlock(&q_lock);
        break;

      case MRT_RCLS :
        pthread_mutex_lock(&q_lock);
          curr_sender = get_item_q(connected_senders_q, sender_matcher, &port_holder);
          if (curr_sender != NULL) {
            // trick the checker into doing clean-up
            curr_sender->inactive_time = TIMEOUT_THRESHOLD;
            // then be polite and do an ACLS
            build_acls();
            sendto(send_sockfd, outgoing_buffer, 
              (MRT_HASH_LENGTH + MRT_TYPE_LENGTH), 0, 
              (const struct sockaddr *)(&addr_holder), addr_len);
          } else {
            /* else the sender is trying to disconnect without being connected;
            * in that case, just try to remove it from the queue...
            */
            pop_item_q(pending_senders_q, sender_matcher, &port_holder);
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
      pthread_cancel(*(curr_sender->checker_thread));
      free(curr_sender);
    }
    delete_q(connected_senders_q, free); // could use free(q) directly
  pthread_mutex_unlock(&q_lock);

  // TODO: signal all blocked mrt_receive1()... or let them wake up from sleep?
  return NULL;
}


/* the main sendner; simply keeps sending DATA: send meaningful
 * DATA when the latest window size is big enough; send empty
 * DATA otherwise.
 */
void *main_sender(void *_null) {
  while 
}

/* should be run as soon as connection is established (first ACON
 * received). Just keeps incrementing the inactivity counter until
 * the connection needs to be dropped...
 */
void *checker(void *sender_vp) {
  sender_t *sender_p = (sender_t *)sender_vp;

  while (1) {
    pthread_mutex_lock(&q_lock);
      // else the sender is responsible; increment inactivity counter
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
    pop_item_q(connected_senders_q, sender_matcher, &(sender_p->addr.sin_port));
  pthread_mutex_unlock(&q_lock);
  free(sender_p);

  return NULL;
}

/****** helper functions (unavailable to module users) ******/

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

// buffer must be at least (MRT_HEADER_LENGTH + 1) in length
void build_adat(char *buffer, int received_frag, int curr_window_size) {
  memmove(buffer + MRT_TYPE_LOCATION, &adat_type, MRT_TYPE_LENGTH);
  memmove(buffer + MRT_FRAGMENT_LOCATION, &received_frag, MRT_FRAGMENT_LENGTH);
  memmove(buffer + MRT_WINDOWSIZE_LOCATION, &curr_window_size, MRT_WINDOWSIZE_LENGTH);
  
  buffer[MRT_HEADER_LENGTH] = '\0';
  unsigned long hash_holder = hash(buffer + MRT_HASH_LENGTH);
  memmove(buffer, &hash_holder, MRT_HASH_LENGTH);
}

void build_acls() {
  memmove(outgoing_buffer + MRT_TYPE_LOCATION, &acls_type, MRT_TYPE_LENGTH);
  
  outgoing_buffer[MRT_TYPE_LENGTH] = '\0';
  unsigned long hash_holder = hash(outgoing_buffer + MRT_HASH_LENGTH);
  memmove(outgoing_buffer, &hash_holder, MRT_HASH_LENGTH);
}

