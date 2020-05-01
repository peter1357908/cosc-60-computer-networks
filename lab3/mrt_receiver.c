/* Receiver functions for the Mini Reliable Transport module.
 *	
 * For Dartmouth COSC 60 Lab 3;
 * By Shengsong Gao, April 2020.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // exit(), malloc(), free()
#include <unistd.h> // close()
#include <sys/socket.h>
#include <arpa/inet.h> // htons()
#include <pthread.h>

#include "mrt.h"
#include "mrt_receiver.h"
#include "Queue.h"
#include "utilities.h" // hash()

#define PORT_NUMBER 4242

/****** declarations ******/
typedef struct sender {
  sockaddr_in addr = {0};
  char buffer[MAX_WINDOW_SIZE] = {0};
  int num_unread_bytes = 0;
  int next_frag = 0;
} sender_t;

void *main_handler(void *);

/****** global variables ******/
const int initial_window_size = MAX_WINDOW_SIZE;
int window_size_holder = 0;
unsigned long hash_holder;

int should_close = 0;
pthread_mutex_t close_lock = PTHREAD_MUTEX_INITIALIZER;

q_t *pending_senders_q;
q_t *connected_senders_q;
pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_t main_thread;
char incoming_buffer[MAX_UDP_PAYLOAD_SIZE + 1]; // +1 for NULL-termination for hash()
char outgoing_buffer[MAX_UDP_PAYLOAD_SIZE + 1]; // +1 for NULL-termination for hash()
sender_t curr_sender;

/****** functions ******/

// will create the main thread that handles all incoming transmissions
int mrt_open() {
  /****** initializing socket and receiver address ******/
  int rece_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (rece_sockfd < 0)
  {
    perror("rece_sockfd = socket() error\n");
    return MRT_ERROR;
  }

  struct sockaddr_in rece_addr = {0};
  rece_addr.sin_family = AF_INET;
  rece_addr.sin_port = htons(PORT_NUMBER);
  rece_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  /****** binding the socket ******/
  if (bind(rece_sockfd, (struct sockaddr *)&rece_addr, sizeof(rece_addr)) < 0)
  {
    perror("bind(rece_sockfd) error\n");
    return MRT_ERROR;
  }

  /****** initiating the main handler ******/
  pthread_mutex_lock(&q_lock);
    pending_senders_q = make_q();
    connected_senders_q = make_q();
    if (pending_senders_q == NULL || connected_senders_q == NULL) {
      perror("make_q() returned NULL\n");
      return MRT_ERROR;
    }
  pthread_mutex_unlock(&q_lock);

  if (pthread_create(&main_thread, NULL, main_handler, NULL) != 0) {
    perror("pthread_create(main_thread) error\n");
    return MRT_ERROR;
  }
  
  return MRT_SUCCESS;
}

/****** thread functions (unavailable to module users) ******/

/* The main handler; all incoming transmissions will be validated
 * and handled here.
 *
 */
void *main_handler(void *_null) {
  unsigned int addr_len = (unsigned int) sizeof(struct sockaddr_in);
  int bytes_received, curr_type, curr_frag;
  unsigned long curr_hash;
  sender_t *new_sender;
  // the main loop; processes all the incoming transmissions
  while (1) {
    num_char_received = recvfrom(rece_sockfd, incoming_buffer,
      MAX_MRT_PAYLOAD_SIZE, 0, (struct sockaddr *)(&(curr_sender.addr)),
      &addr_len);

    // before processing, check if close is flagged
    pthread_mutex_lock(&close_lock);
      if (should_close == 1) {
        printf("Breaking from main loop because should_close\n");
        break;
      }
    pthread_mutex_unlock(&close_lock);

    // NULL-terminate the transmission to enable hash()
    incoming_buffer[num_char_received] = '\0';

    // first validate the transmission with checksum
    curr_hash = *((unsigned long *)incoming_buffer);
    if (hash(&(incoming_buffer[MRT_HASH_LENGTH])) != curr_hash) {
      printf("transmission discarded due to checksum mismatch\n");
      continue;
    }

    // then check the transmission type and act accordingly
    memmove(&curr_type, incoming_buffer + MRT_TYPE_LOCATION, sizeof(int));
    switch (curr_type) {
      
      case MRT_RCON :
        // if the sender is not queued...
        if (get_item_q(connected_senders_q, sender_matcher, &curr_sender) == NULL) {
          // queue the sender if it also not connected yet (new sender)
          if (get_item_q(pending_senders_q, sender_matcher, &curr_sender) == NULL) {
            pthread_mutex_lock(&q_lock);
              new_sender = malloc(sizeof(sender_t));
              memmove(new_sender, &curr_sender, sizeof(sender_t));
              enq_q(pending_senders_q, new_sender);
            pthread_mutex_unlock(&q_lock);
          }
          // if it is already connected, send a (duplicate) ACON
          else {
            build_acon(outgoing_buffer, &curr_sender);
            // TODO: use MSG_CONFIRM flag?
            // TODO: send via another thread?
            sendto(rece_sockfd, outgoing_buffer, MRT_HEADER_LENGTH,  
              0, (const struct sockaddr *)(&(curr_sender.addr)), 
              addr_len);
          }
        }
        // else the sender is queued, and must not be already connected
        // do nothing
        break;

      case MRT_DATA :
        
        break;

      case MRT_RCLS :

        break;

      default :
        // ACON, ADAT, ACLS, UNKN
        printf("transmission discarded due to bad type = %d\n", curr_type);
        continue;
    }


  }

  // TODO: clean-up procedure
}


/****** helper functions (unavailable to module users) ******/

/* comparison function for senders
 *
 * returns 1 if the two senders have the same port number
 * returns 0 otherwise
 *
 * designed to be used as a Queue module callback function
 */
int sender_matcher(void *existing_sender_vp, void *current_sender_vp) {
  sender_t *es = (sender_t *)existing_sender_vp;
  sender_t *cs = (sender_t *)current_sender_vp;

  return (es->addr.sin_port == cs->addr.sin_port);
}

// Assumes that memmove() always succeeds
// TODO: write templates in mrt.h for repeating info?
// TODO: pointless sizeof() because of pre-determined length
void build_acon(char *buffer, sender_t *sender) {
	memmove(outgoing_buffer + MRT_TYPE_LOCATION, &acon_type, sizeof(int));
  memmove(outgoing_buffer + MRT_FRAGMENT_LOCATION, &(sender.next_frag), sizeof(int));
  memmove(outgoing_buffer + MRT_ID_LOCATION, &(sender.addr.sin_port), sizeof(in_port_t));
  memmove(outgoing_buffer + MRT_WINDOWSIZE_LOCATION, &initial_window_size, sizeof(int));

  outgoing_buffer[MRT_HEADER_LENGTH] = '\0';
  hash_holder = hash(outgoing_buffer);
  memmove(outgoing_buffer, &hash_holder, sizeof(unsigned long));
}