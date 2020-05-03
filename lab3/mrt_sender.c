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

#define RCON_PERIOD               EXPECTED_RTT * 2
#define EMPTY_DATA_PERIOD         EXPECTED_RTT * 2
#define RESEND_TIMEOUT_THRESHOLD  EMPTY_DATA_PERIOD * 3
#define CLOSE_TIMEOUT_INCREMENT   EMPTY_DATA_PERIOD * 2 // timeout increment
#define CLOSE_TIMEOUT_THRESHOLD   CLOSE_TIMEOUT_INCREMENT * 3
#define MAX_PAYLOADS_BUFFERABLE   10

/****** declarations ******/
typedef struct connection {
  int id;
  struct sockaddr_in send_addr;
  struct sockaddr_in rece_addr;

  /* note that the two arrays below only have valid elements in index
  * up to the last_payload_index (should not access anything beyond it)
  */
  int last_payload_index; // cannot go over MAX_PAYLOADS_BUFFERABLE
  char sender_buffer[MAX_MRT_PAYLOAD_LENGTH * MAX_PAYLOADS_BUFFERABLE];
  int num_bytes_buffered[MAX_PAYLOADS_BUFFERABLE];
  pthread_mutex_t buffer_lock;

  /* always 1 lower than oldest buffered fragment;
   * initially -1, and set to 0 upon first ACON to indict a connection
   * is formed.
   */
  int last_acknowledged_frag;
  int receiver_window_size;
  pthread_mutex_t receiver_lock;

  /* assumes that the first frag is number 0 and is already acknowledged
   * before being used (after the first ACON, thus beginning with 0)
   */
  int last_sent_frag;

  int inactive_time;
  pthread_mutex_t timeout_lock;

  int should_close;
  pthread_mutex_t close_lock;

  pthread_t handler_thread, sender_thread, checker_thread;

  char incoming_buffer[MRT_HEADER_LENGTH + 1]; // +1 for NULL-termination for hash()
  char outgoing_buffer[MAX_UDP_PAYLOAD_LENGTH + 1];
  pthread_mutex_t outgoing_lock;
} connection_t;

void *handler(void *conn_vp);
void *sender(void *conn_vp);
void *checker(void *conn_vp);
void build_rcon(char *outgoing_buffer);
void build_data_empty(char *outgoing_buffer);
void build_data(connection_t *conn_p, int payload_index, int len);
void build_rcls(char *outgoing_buffer);

/****** global variables ******/
unsigned int addr_len = (unsigned int) sizeof(struct sockaddr_in);
int next_id = 0;
int initial_frag = 0;

q_t *connections_q = NULL;
pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;

/****** functions ******/

/* returns the connection ID (int; positive)
 * returns -1 upon any error and 0 upon success
 * will block until the connection is established
 *
 * the `s_addr` should have the same format as
 * `(struct sockaddr_in).sin_addr.s_addr`
 * `s_addr` will be put inside `htonl()` before use
 */
int mrt_connect(unsigned short port_number, unsigned long s_addr) {
  /****** initializing the module if not done so yet ******/
  pthread_mutex_lock(&q_lock);
  if (connections_q == NULL) {
    connections_q = make_q();
    if (connections_q == NULL) {
      perror("make_q() failed\n");
      return -1;
    }
  }
  pthread_mutex_unlock(&q_lock);
  
  /****** initialize a new connection struct and queue it ******/
  connection_t *curr_conn = connection_t_init(port_number, s_addr);
  if (curr_conn == NULL) {
    perror ("connection_t_init() failed\n");
    return -1;
  }
  
  pthread_mutex_lock(&q_lock);
  enq_q(connections_q, curr_conn);
  pthread_mutex_unlock(&q_lock);

  /****** just keep trying to connect to server... ******/

  // create the handler thread first (or else ACON cannot be handled)
  if (pthread_create(&(curr_conn->handler_thread), NULL, handler, curr_conn) != 0) {
    perror("pthread_create(handler) error\n");
    return -1;
  }
 
  while(1) {
    pthread_mutex_lock(&(curr_conn->lock));
    if (curr_conn->id != -1) {
      pthread_mutex_unlock(&(curr_conn->lock));
      break;
    }

    build_rcon(curr_conn->outgoing_buffer);
    sendto(curr_conn->send_sockfd, curr_conn->outgoing_buffer,
          MRT_HASH_LENGTH + MRT_TYPE_LENGTH + MRT_FRAGMENT_LENGTH  
          0, (const struct sockaddr *)(&(curr_conn->rece_addr)), 
          addr_len);
    pthread_mutex_unlock(&(curr_conn->lock));
    sleep(RCON_PERIOD);
  }
  
  return id;
}

/* returns the number of bytes successfully sent (acknowledged).
 * will block until the corresponding final ADAT is processed (large
 * enough data will be split into multiple fragments)
 *
 * Returns -1 if the call is spurious (connection not accepted yet,
 * mrt_open() not even called yet, etc.)
 */
int mrt_send(void *buffer, int len) {
  int bytes_sent;

  return bytes_sent;
}

/* will wait until final ADAT is received to send a RCLS
 * (unless signaled to close by timeout). Blocking.
 */
void mrt_disconnect(){

}

/****** thread functions (unavailable to module users) ******/

/* The main handler; all incoming transmissions will be validated
 * and handled here.
 */
void *handler(void *conn_vp) {
  connection_t *conn_p = (connection_t *)conn_vp;
  int num_bytes_received = 0;
  struct sockaddr_in addr_holder = {0}; // to hold the addr of incoming transmission
  unsigned long hash_holder = 0;
  unsigned int addr_len_holder = 0;
  int type_holder = 0, frag_holder = 0, winsize_holder = 0;
  int frag_difference = 0, num_remaining_bytes = 0;
  char *remaining_bytes_location = NULL;


  // the main loop; processes all the incoming transmissions
  while (1) {
    pthread_mutex_lock(&(curr_conn->lock));
    num_bytes_received = recvfrom(conn_p->send_sockfd, conn_p->incoming_buffer,
                      MRT_HEADER_LENGTH, 0, (struct sockaddr *)(&addr_holder),
                      &addr_len_holder);

    // before processing, check if close is flagged
    if (should_close == 1) {
      printf("Breaking from handler loop because should_close\n");
      break;
    }

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
            pthread_create(&sender_thread, NULL, sender, NULL);
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
          inactive_time = CLOSE_TIMEOUT_THRESHOLD + 1;
        pthread_mutex_unlock(&timeout_lock);
        break;

      default :
        // RCON, DATA, RCLS, UNKN
        printf("transmission had good checksum but bad type = %d\n HOW????\n", type_holder);
        continue;
    }
    pthread_mutex_unlock(&(curr_conn->lock));
  }
  /* Should be closing now. Any clean-ups?
   */
  pthread_join(checker_thread, NULL);
  pthread_join(sender_thread, NULL);

  // TODO: signal all blocked mrt_send()... or let them wake up from sleep?
  return NULL;
}

/* the main sender; simply keeps sending DATA:
 * if the receiver window_size is too small or all data sent:
 *   if all data sent:
 *     start a timer... once threshold exceeded, start re-sending old
 *     payloads (by marking them as unsent)
 *   send empty DATA
 * else:
 *    send the next payload in the buffer
 */
void *sender(void *conn_vp) {
  int resend_time = 0;

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
        if (next_payload_index > last_payload_index) {
          if (resend_time > RESEND_TIMEOUT_THRESHOLD) {
            last_sent_frag = last_acknowledged_frag;
            resend_time = 0;
          } else { resend_time += EMPTY_DATA_PERIOD; }
        } 
        else {
          // the sender has something to send; reset timer
          resend_time = 0; 
        }
        // send empty DATA and sleep
    pthread_mutex_unlock(&buffer_lock);
    pthread_mutex_unlock(&receiver_lock);
        pthread_mutex_lock(&outgoing_lock);
          build_data_empty();
          sendto(send_sockfd, outgoing_buffer, MRT_HEADER_LENGTH,  
                  0, (const struct sockaddr *)(&rece_addr), 
                  addr_len);
        pthread_mutex_lock(&outgoing_lock);
        sleep(EMPTY_DATA_PERIOD);
      } else {
        // the sender has something to send; reset timer
        resend_time = 0;
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
void *checker(void *conn_vp) {
  while (1) {
    pthread_mutex_lock(&timeout_lock);
      inactive_time += CLOSE_TIMEOUT_INCREMENT;
      // if it would sleep past the threshold, go BOOM
      if (inactive_time > CLOSE_TIMEOUT_THRESHOLD) {
    pthread_mutex_unlock(&timeout_lock);
        pthread_mutex_lock(&close_lock);
          should_close = 1;
        pthread_mutex_unlock(&close_lock);
        break;
      }
    pthread_mutex_unlock(&timeout_lock);
    sleep(CLOSE_TIMEOUT_INCREMENT);
  }
  // TODO: any garbage collection necessary?

  return NULL;
}

/****** helper functions (unavailable to module users) ******/

/* initialize a new connection struct and returns its pointer
 * the caller is responsible for freeing it.
 */
connection_t *connection_t_init(unsigned short port_number, unsigned long s_addr) {
  connection_t *connection_p = calloc(1, sizeof(connection_t));

  if (connection_p == NULL) { return NULL }

  if (pthread_mutex_init(&(connection_p->buffer_lock), NULL) != 0 ||
      pthread_mutex_init(&(connection_p->receiver_lock), NULL) != 0 ||
      pthread_mutex_init(&(connection_p->timeout_lock), NULL) != 0 ||
      pthread_mutex_init(&(connection_p->close_lock), NULL) != 0 ||
      pthread_mutex_init(&(connection_p->outgoing_lock), NULL) != 0
      ) { return NULL }

  connection_p->send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (connection_p->send_sockfd < 0) { return NULL }

  connection_p->id = next_id++;
  
  connection_p->rece_addr.sin_family = AF_INET;
  connection_p->rece_addr.sin_port = htons(port_number);
  connection_p->rece_addr.sin_addr.s_addr = htonl(s_addr);

  connection_p->last_payload_index = -1;
  
  connection_p->receiver_window_size = 0;
  connection_p->last_acknowledged_frag = -1;

  connection_p->last_sent_frag = 0;

  connection_p->inactive_time = 0;

  connection_p->should_close = 0;

  return connection_p;
}

/* returns 1 if the connection's id matches the input id;
 * returns 0 otherwise.
 *
 * designed to be used as a Queue module callback function
 */
int connection_matcher(void *connection_vp, void *id_vp) {
  connection_t *connection_p = (connection_t *)connection_vp;
  int *id_p = (int *)id_vp;

  if (connection_p->id == *id_p) {
    return 1;
  }
  return 0;
}

/* the build_x() functions assume that memmove() always succeeds
 * and need to be inside the respective connection's mutex pair
 */
void build_rcon(char *outgoing_buffer) {
  memmove(outgoing_buffer + MRT_TYPE_LOCATION, &rcon_type, MRT_TYPE_LENGTH);
  memmove(outgoing_buffer + MRT_FRAGMENT_LOCATION, &initial_frag, MRT_FRAGMENT_LENGTH);
  
  outgoing_buffer[MRT_HASH_LENGTH + MRT_TYPE_LENGTH + MRT_FRAGMENT_LENGTH] = '\0';
  unsigned long hash_holder = hash(outgoing_buffer + MRT_HASH_LENGTH);
  memmove(outgoing_buffer, &hash_holder, MRT_HASH_LENGTH);
}

void build_data_empty(char *outgoing_buffer) {
  // choose a fake_frag such that the sender will treat it as droppable
  int fake_frag = -1;
  memmove(outgoing_buffer + MRT_TYPE_LOCATION, &data_type, MRT_TYPE_LENGTH);
  memmove(outgoing_buffer + MRT_FRAGMENT_LOCATION, &fake_frag, MRT_FRAGMENT_LENGTH);
  
  outgoing_buffer[MRT_HASH_LENGTH + MRT_TYPE_LENGTH + MRT_FRAGMENT_LENGTH] = '\0';
  unsigned long hash_holder = hash(outgoing_buffer + MRT_HASH_LENGTH);
  memmove(outgoing_buffer, &hash_holder, MRT_HASH_LENGTH);
}

void build_data(connection_t *conn_p, int payload_index, int len) {
  int last_acknowledged_frag = conn_p->last_acknowledged_frag;
  char *sender_buffer = conn_p->sender_buffer;
  char *outgoing_buffer = conn_p->outgoing_buffer;

  int sending_frag = last_acknowledged_frag + payload_index + 1;

  memmove(outgoing_buffer + MRT_TYPE_LOCATION, &data_type, MRT_TYPE_LENGTH);
  memmove(outgoing_buffer + MRT_FRAGMENT_LOCATION, &sending_frag, MRT_FRAGMENT_LENGTH);

  memmove(outgoing_buffer + MRT_PAYLOAD_LOCATION, sender_buffer + payload_index * MAX_MRT_PAYLOAD_LENGTH, len);
  
  
  outgoing_buffer[MRT_PAYLOAD_LOCATION + len] = '\0';
  unsigned long hash_holder = hash(outgoing_buffer + MRT_HASH_LENGTH);
  memmove(outgoing_buffer, &hash_holder, MRT_HASH_LENGTH);
}

/* no need to keep track of the fragment number here... only sent
 * after the last expected ADAT is received
 */
void build_rcls(char *outgoing_buffer) {
  memmove(outgoing_buffer + MRT_TYPE_LOCATION, &rcls_type, MRT_TYPE_LENGTH);
  
  outgoing_buffer[MRT_HASH_LENGTH + MRT_TYPE_LENGTH] = '\0';
  unsigned long hash_holder = hash(outgoing_buffer + MRT_HASH_LENGTH);
  memmove(outgoing_buffer, &hash_holder, MRT_HASH_LENGTH);
}

