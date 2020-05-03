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
#define SENDER_PERIOD           500
#define RCON_PERIOD             500
#define TIMEOUT_THRESHOLD       (CHECKER_PERIOD * 5)
#define MAX_PAYLOADS_BUFFERABLE 10

/****** declarations ******/

void *main_handler(void *_null);
void *main_sender(void *_null);
void *checker(void *_null);
void build_rcon();
void build_data_empty();
void build_data(int payload_index, int len);
void build_rcls();

/****** global variables ******/
unsigned int addr_len = (unsigned int) sizeof(struct sockaddr_in);
int send_sockfd;
struct sockaddr_in rece_addr = {0};

unsigned short *id_p = NULL; // indicative of whether a connection is formed
pthread_mutex_t id_lock = PTHREAD_MUTEX_INITIALIZER;

int receiver_window_size = 0;
int last_acknowledged_frag = 0; // always 1 lower than oldest buffered payload
pthread_mutex_t receiver_lock = PTHREAD_MUTEX_INITIALIZER;

int inactive_time = 0;
pthread_mutex_t timeout_lock = PTHREAD_MUTEX_INITIALIZER;

int should_close = 0;
pthread_mutex_t close_lock = PTHREAD_MUTEX_INITIALIZER;

/* only changed in sender_thread; no mutex needed
 *
 * assumes that the first frag is number 0 and is already acknowledged
 * before being used (after the first ACON, thus beginning with 0)
 */
int last_sent_frag = 0;

/* note that the two arrays below only have valid elements in index
 * up to the last_payload_index (should not access anything beyond it)
 */
int last_payload_index = -1; // cannot go over MAX_PAYLOADS_BUFFERABLE
char sender_buffer[MAX_MRT_PAYLOAD_LENGTH * MAX_PAYLOADS_BUFFERABLE];
int num_bytes_buffered[MAX_PAYLOADS_BUFFERABLE];
pthread_mutex_t buffer_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_t handler_thread, sender_thread, checker_thread;
char incoming_buffer[MRT_HEADER_LENGTH + 1]; // +1 for NULL-termination for hash()
char outgoing_buffer[MAX_UDP_PAYLOAD_LENGTH + 1]; // +1 for NULL-termination for hash()
pthread_mutex_t outgoing_lock = PTHREAD_MUTEX_INITIALIZER;

/****** functions ******/

/* returns a pointer to the connection ID; the caller is
 * responsible for freeing it.
 */
unsigned short *mrt_connect() {
  /****** initializing socket and receiver address ******/
  send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (send_sockfd < 0)
  {
    perror("send_sockfd = socket() error\n");
    return NULL;
  }

  rece_addr.sin_family = AF_INET;
  rece_addr.sin_port = htons(PORT_NUMBER);
  rece_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  /****** just keep trying to connect to server... ******/

  // create the handler thread first (or else ACON cannot be handled)
  if (pthread_create(&handler_thread, NULL, main_handler, NULL) != 0) {
    perror("pthread_create(main_thread) error\n");
    return NULL;
  }
 
  while(1) {
    pthread_mutex_lock(&id_lock);
      if (id_p != NULL) {
        pthread_mutex_unlock(&id_lock);
        pthread_mutex_destroy(&id_lock);
        break;
      }
    pthread_mutex_unlock(&id_lock);
    
    pthread_mutex_lock(&outgoing_lock);
      build_rcon();
      sendto(send_sockfd, outgoing_buffer, MRT_HEADER_LENGTH,  
            0, (const struct sockaddr *)(&rece_addr), 
            addr_len);
    pthread_mutex_lock(&outgoing_lock);
    sleep(RCON_PERIOD);
  }
  
  return id_p;
}


/****** thread functions (unavailable to module users) ******/

/* The main handler; all incoming transmissions will be validated
 * and handled here.
 */
void *main_handler(void *_null) {
  int num_bytes_received = 0;
  struct sockaddr_in addr_holder = {0}; // to hold the addr of incoming transmission
  unsigned long hash_holder = 0;
  int type_holder = 0, frag_holder = 0, winsize_holder = 0;
  int frag_difference = 0, num_remaining_bytes = 0;
  char *remaining_bytes_location = NULL;

  // the main loop; processes all the incoming transmissions
  while (1) {
    num_bytes_received = recvfrom(send_sockfd, incoming_buffer,
      MRT_HEADER_LENGTH, 0, (struct sockaddr *)(&addr_holder),
      &addr_len);

    // before processing, check if close is flagged
    pthread_mutex_lock(&close_lock);
      if (should_close == 1) {
        printf("Breaking from handler loop because should_close\n");
    pthread_mutex_unlock(&close_lock);
        break;
      }
    pthread_mutex_unlock(&close_lock);

    // NULL-terminate the transmission to enable hash()
    incoming_buffer[num_bytes_received] = '\0';

    // first validate the transmission with checksum
    memmove(&hash_holder, incoming_buffer, MRT_HASH_LENGTH);
    if (hash(incoming_buffer + MRT_HASH_LENGTH) != hash_holder) {
      printf("transmission discarded due to checksum mismatch\n");
      continue;
    }

    // then check the transmission type and act accordingly
    memmove(&type_holder, incoming_buffer + MRT_TYPE_LOCATION, MRT_TYPE_LENGTH);
    memmove(&frag_holder, incoming_buffer + MRT_FRAGMENT_LOCATION, MRT_FRAGMENT_LENGTH);
    memmove(&winsize_holder, incoming_buffer + MRT_WINDOWSIZE_LOCATION, MRT_WINDOWSIZE_LENGTH);
    switch (type_holder) {
      
      case MRT_ACON :
        // start the sender_thread if it hasn't yet (meaning first ACON)
        // TODO: what if pthread_create() fails?
        pthread_mutex_lock(&id_lock);
          if (id_p == NULL) {
            pthread_create(&sender_thread, NULL, main_sender, NULL);
            pthread_create(&checker_thread, NULL, checker, NULL);
            id_p = malloc(sizeof(unsigned short));
            *id_p = addr_holder.sin_port;
          }
        pthread_mutex_unlock(&id_lock);
        // otherwise do nothing (duplicate ACONs are ignored)
        break;

      case MRT_ADAT :
        // first of all, receiver just proved the connection is alive
        pthread_mutex_lock(&timeout_lock);
          inactive_time = 0;
        pthread_mutex_unlock(&timeout_lock);
        
        /* if the ADAT is at least as new as the last one, update
         * the frag and receiver_window_size if necessary
         */
        pthread_mutex_lock(&receiver_lock);
          frag_difference = frag_holder - last_acknowledged_frag;
          if (frag_difference >= 0) {
            last_acknowledged_frag = frag_holder;
            if (receiver_window_size < winsize_holder) {
              receiver_window_size = winsize_holder;
            }
          }
        pthread_mutex_unlock(&receiver_lock);

        // if we can free up the buffer, do it
        // note that frag_difference here does not need to be in mutex
        if (frag_difference > 0) {
          pthread_mutex_lock(&buffer_lock);
            // update the buffer
            remaining_bytes_location = sender_buffer + MAX_MRT_PAYLOAD_LENGTH * frag_difference;
            num_remaining_bytes = MAX_MRT_PAYLOAD_LENGTH * (last_payload_index - frag_difference + 1);
            memmove(sender_buffer, remaining_bytes_location, num_remaining_bytes);
            // update the num_bytes_buffered array
            remaining_bytes_location = (char *)(num_bytes_buffered + frag_difference);
            num_remaining_bytes = sizeof(int) * (last_payload_index - frag_difference + 1);
            memmove(num_bytes_buffered, remaining_bytes_location, num_remaining_bytes);
            // update the last_payload_index
            last_payload_index -= frag_difference;
          pthread_mutex_unlock(&buffer_lock);
        }
        break;

      case MRT_ACLS :
        // could just do nothing here but...
        pthread_mutex_lock(&timeout_lock);
          inactive_time = TIMEOUT_THRESHOLD + 1;
        pthread_mutex_unlock(&timeout_lock);
        break;

      default :
        // RCON, DATA, RCLS, UNKN
        printf("transmission had good checksum but bad type = %d\n HOW????\n", type_holder);
        continue;
    }
  }
  /* Should be closing now. Any clean-ups?
   */
  pthread_join(checker_thread, NULL);
  pthread_join(sender_thread, NULL);

  // TODO: signal all blocked mrt_send()... or let them wake up from sleep?
  return NULL;
}


/* the main sendner; simply keeps sending DATA: send empty DATA
 * and go to sleep for a while if:
 * 1. the receiver_window_size is too small
 * 2. all frags in the buffer are sent (empty buffer is a special case of 2.)
 */
void *main_sender(void *_null) {
  while (1) {
    pthread_mutex_lock(&close_lock);
      if (should_close == 1) {
    pthread_mutex_unlock(&close_lock);
        break;
      }
    pthread_mutex_unlock(&close_lock);

    
    // TODO: simplify dangerously nested mutex
    pthread_mutex_lock(&receiver_lock);
    pthread_mutex_lock(&buffer_lock);
      int next_payload_index = last_sent_frag - last_acknowledged_frag;
      if (receiver_window_size < MAX_MRT_PAYLOAD_LENGTH 
          || next_payload_index > last_payload_index) {
        // send empty DATA
    pthread_mutex_unlock(&buffer_lock);
    pthread_mutex_unlock(&receiver_lock);
        pthread_mutex_lock(&outgoing_lock);
          build_data_empty();
          sendto(send_sockfd, outgoing_buffer, MRT_HEADER_LENGTH,  
                  0, (const struct sockaddr *)(&rece_addr), 
                  addr_len);
        pthread_mutex_lock(&outgoing_lock);
        sleep(SENDER_PERIOD);
      } else {
        // send meaningful DATA
        pthread_mutex_lock(&outgoing_lock);
          build_data(next_payload_index, num_bytes_buffered[next_payload_index]);
          sendto(send_sockfd, outgoing_buffer, MRT_HEADER_LENGTH,  
                  0, (const struct sockaddr *)(&rece_addr), 
                  addr_len);
        pthread_mutex_lock(&outgoing_lock);
    pthread_mutex_unlock(&buffer_lock);
    pthread_mutex_unlock(&receiver_lock);
        last_sent_frag += 1;
      }
  }

  // TODO: clean-ups?
  return NULL;
}

/* should be run as soon as connection is established (first ACON
 * received). Just keeps incrementing the inactivity counter until
 * the connection needs to be dropped...
 */
void *checker(void *_null) {
  while (1) {
    pthread_mutex_lock(&timeout_lock);
      inactive_time += CHECKER_PERIOD;
      // if it would sleep past the threshold, go BOOM
      if (inactive_time > TIMEOUT_THRESHOLD) {
    pthread_mutex_unlock(&timeout_lock);
        pthread_mutex_lock(&close_lock);
          should_close = 1;
        pthread_mutex_unlock(&close_lock);
        break;
      }
    pthread_mutex_unlock(&timeout_lock);
    sleep(CHECKER_PERIOD);
  }
  // TODO: any garbage collection necessary?

  return NULL;
}

/****** helper functions (unavailable to module users) ******/

// the build_x() functions assume that memmove() always succeeds
void build_rcon() {
  int initial_frag = 0;
  memmove(outgoing_buffer + MRT_TYPE_LOCATION, &rcon_type, MRT_TYPE_LENGTH);
  memmove(outgoing_buffer + MRT_FRAGMENT_LOCATION, &initial_frag, MRT_FRAGMENT_LENGTH);
  
  outgoing_buffer[MRT_HASH_LENGTH + MRT_TYPE_LENGTH + MRT_FRAGMENT_LENGTH] = '\0';
  unsigned long hash_holder = hash(outgoing_buffer + MRT_HASH_LENGTH);
  memmove(outgoing_buffer, &hash_holder, MRT_HASH_LENGTH);
}

void build_data_empty() {
  // choose a fake_frag such that the sender will treat it as droppable
  int fake_frag = -1;
  memmove(outgoing_buffer + MRT_TYPE_LOCATION, &data_type, MRT_TYPE_LENGTH);
  memmove(outgoing_buffer + MRT_FRAGMENT_LOCATION, &fake_frag, MRT_FRAGMENT_LENGTH);
  
  outgoing_buffer[MRT_HASH_LENGTH + MRT_TYPE_LENGTH + MRT_FRAGMENT_LENGTH] = '\0';
  unsigned long hash_holder = hash(outgoing_buffer + MRT_HASH_LENGTH);
  memmove(outgoing_buffer, &hash_holder, MRT_HASH_LENGTH);
}

// expects to be wrapped around a buffer_lock and a receiver_lock
void build_data(int payload_index, int len) {
  int sending_frag = last_acknowledged_frag + payload_index + 1;

  memmove(outgoing_buffer + MRT_TYPE_LOCATION, &data_type, MRT_TYPE_LENGTH);
  memmove(outgoing_buffer + MRT_FRAGMENT_LOCATION, &sending_frag, MRT_FRAGMENT_LENGTH);

  memmove(outgoing_buffer + MRT_PAYLOAD_LOCATION, sender_buffer + payload_index * MAX_MRT_PAYLOAD_LENGTH, len);
  
  
  outgoing_buffer[MRT_HEADER_LENGTH + len] = '\0';
  unsigned long hash_holder = hash(outgoing_buffer + MRT_HASH_LENGTH);
  memmove(outgoing_buffer, &hash_holder, MRT_HASH_LENGTH);
}

/* no need to keep track of the fragment number here... only sent
 * after the last expected ADAT is received
 */
void build_rcls() {
  memmove(outgoing_buffer + MRT_TYPE_LOCATION, &rcls_type, MRT_TYPE_LENGTH);
  
  outgoing_buffer[MRT_HASH_LENGTH + MRT_TYPE_LENGTH] = '\0';
  unsigned long hash_holder = hash(outgoing_buffer + MRT_HASH_LENGTH);
  memmove(outgoing_buffer, &hash_holder, MRT_HASH_LENGTH);
}

