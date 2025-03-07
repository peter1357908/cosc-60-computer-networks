/* Receiver functions for the Mini Reliable Transport module.
 *	
 * For Dartmouth COSC 60 Lab 3;
 * By Shengsong Gao, April 2020.
 */

// the following two includes are necessary for usleep()
#define _XOPEN_SOURCE   600
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // exit(), calloc(), free()
#include <unistd.h> // close(), usleep()
#include <sys/socket.h>
#include <arpa/inet.h> // htons()
#include <pthread.h>

#include "mrt.h"
#include "mrt_sender.h"
#include "Queue.h"
#include "utilities.h" // hash()

#define RCON_PERIOD               EXPECTED_RTT * 2
#define EMPTY_DATA_PERIOD         EXPECTED_RTT * 2
#define MRT_SEND_PERIOD           EXPECTED_RTT * 2
#define MRT_DISCONNECT_PERIOD     EXPECTED_RTT * 4
#define RESEND_TIMEOUT_THRESHOLD  EMPTY_DATA_PERIOD * 3
#define CLOSE_TIMEOUT_INCREMENT   EMPTY_DATA_PERIOD * 2 // timeout increment
#define CLOSE_TIMEOUT_THRESHOLD   CLOSE_TIMEOUT_INCREMENT * 3
#define MAX_PAYLOADS_BUFFERABLE   10

/****** declarations ******/
typedef struct connection {
  int id;
  int send_sockfd;
  struct sockaddr_in send_addr;  // bind to this address; listening on it
  struct sockaddr_in rece_addr;  // send data to this address

  int last_sent_index;
  int last_payload_index; // must be below MAX_PAYLOADS_BUFFERABLE
  char sender_buffer[MAX_MRT_PAYLOAD_LENGTH * MAX_PAYLOADS_BUFFERABLE];
  int num_bytes_buffered[MAX_PAYLOADS_BUFFERABLE];
  pthread_mutex_t buffer_lock;
  /* note that the two arrays above only have valid elements in index
   * up to the last_payload_index (should not access anything beyond it)
   */

  /* always 1 lower than oldest buffered fragment;
   * initially -1, and set to 0 upon first ACON to indict a connection
   * is formed.
   *
   * WARNING:
   * last_acknowledged_frag changing would require the buffer
   * variables changing as well, making it necessary to wrap the whole
   * ordeal inside a buffer_lock nested INSIDE a receiver_lock
   *
   * TODO: enough to just put last_acknowledged_frag under buffer_lock?
   */
  int last_acknowledged_frag;
  int receiver_window_size;
  pthread_mutex_t receiver_lock;

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
connection_t *connection_t_init(unsigned short sender_port_number, unsigned short receiver_port_number, unsigned long receiver_s_addr);
void connection_t_free(void *conn_vp);
int connection_matcher(void *connection_vp, void *id_vp);
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

/* returns the connection ID (int; non-negative)
 * returns -1 upon any error
 * will block until the connection is established
 *
 * the `s_addr` should have the same format as
 * `(struct sockaddr_in).sin_addr.s_addr`
 * `s_addr` will be put inside `htonl()` before use
 */
int mrt_connect(unsigned short sender_port_number, unsigned short receiver_port_number, unsigned int s_addr) {
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
  connection_t *curr_conn = connection_t_init(sender_port_number, receiver_port_number, s_addr);
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
    pthread_mutex_lock(&(curr_conn->receiver_lock));
    if (curr_conn->last_acknowledged_frag != -1) {
      pthread_mutex_unlock(&(curr_conn->receiver_lock));
      break;
    }
    pthread_mutex_unlock(&(curr_conn->receiver_lock));

    pthread_mutex_lock(&(curr_conn->outgoing_lock));
    build_rcon(curr_conn->outgoing_buffer);
    sendto(curr_conn->send_sockfd, curr_conn->outgoing_buffer,
          MRT_HASH_LENGTH + MRT_TYPE_LENGTH + MRT_FRAGMENT_LENGTH,
          0, (const struct sockaddr *)(&(curr_conn->rece_addr)), 
          addr_len);
    pthread_mutex_unlock(&(curr_conn->outgoing_lock));

    usleep(RCON_PERIOD);
  }
  
  return curr_conn->id;
}

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
int mrt_send(int id, char *buffer, int len) {
  connection_t *conn_p = NULL;
  pthread_mutex_lock(&q_lock);
  conn_p = get_item_q(connections_q, connection_matcher, &id);
  if (conn_p == NULL) {
    pthread_mutex_unlock(&q_lock);
    printf("mrt_send(): spurious call with id=%d.\n", id);
    return -1; 
  }
  pthread_mutex_unlock(&q_lock);

  // DANGEROUS: nested mutex... receiver_lock, then buffer_lock!
  pthread_mutex_lock(&(conn_p->receiver_lock));
  pthread_mutex_lock(&(conn_p->buffer_lock));
  int last_buffered_frag = conn_p->last_payload_index + conn_p->last_acknowledged_frag + 1;
  pthread_mutex_unlock(&(conn_p->buffer_lock));
  pthread_mutex_unlock(&(conn_p->receiver_lock));

  // magic ceiling division: https://stackoverflow.com/a/14878734
  // NOTE: no need to wrap in mutex... right?
  int final_frag = last_buffered_frag + len / MAX_MRT_PAYLOAD_LENGTH + (len % MAX_MRT_PAYLOAD_LENGTH != 0);

  int num_free_payload_spaces;
  int num_bytes_to_copy=0, num_bytes_remaining=len, num_bytes_copied=0;
  char *first_free_space=NULL, *first_byte_to_copy=NULL;
  while (1) {
    // get it again to ensure the connection is still valid
    pthread_mutex_lock(&q_lock);
    conn_p = get_item_q(connections_q, connection_matcher, &id);
    if (conn_p == NULL) {
      // the conn_p is NULL now... after not being NULL once...
      pthread_mutex_unlock(&q_lock);
      printf("sender %d: connection dropped before all data are sent.\n", id);
      // TODO: anyway to tell how many bytes are acknowledged?
      return 0;  
    }
    pthread_mutex_unlock(&q_lock);

    // if the final_frag is acknowledged, time to skedaddle
    pthread_mutex_lock(&(conn_p->receiver_lock));
    if (conn_p->last_acknowledged_frag >= final_frag) {
      pthread_mutex_unlock(&(conn_p->receiver_lock));
      break;
    }
    pthread_mutex_unlock(&(conn_p->receiver_lock));

    // if not done copying yet
    if (num_bytes_copied < len) {
      // copy to the buffer unless not enough space remaining...
      pthread_mutex_lock(&(conn_p->buffer_lock));
      num_free_payload_spaces = MAX_PAYLOADS_BUFFERABLE - (conn_p->last_payload_index + 1);
      if (num_free_payload_spaces > 0) {
        // TODO: copy more than 1 payload at a time
        first_free_space = conn_p->sender_buffer + (conn_p->last_payload_index + 1) * MAX_MRT_PAYLOAD_LENGTH;
        first_byte_to_copy = buffer + num_bytes_copied;
        num_bytes_remaining = len - num_bytes_copied;
        if (num_bytes_remaining > MAX_MRT_PAYLOAD_LENGTH) {
          num_bytes_to_copy = MAX_MRT_PAYLOAD_LENGTH;
        } else {
          num_bytes_to_copy = num_bytes_remaining;
        }
        memmove(first_free_space, first_byte_to_copy, num_bytes_to_copy);
        num_bytes_copied += num_bytes_to_copy;
        conn_p->last_payload_index += 1;
        conn_p->num_bytes_buffered[conn_p->last_payload_index] = num_bytes_to_copy;
      }
      pthread_mutex_unlock(&(conn_p->buffer_lock));
    } else {
      // done copying already; go to sleep...
      usleep(MRT_SEND_PERIOD);
      continue; // just to be safe
    }
  }
  // TODO: num_bytes_copied SHOULD be equal to len now...
  return 1;
}

/* will wait until final ADAT is received to send a RCLS
 * (unless signaled to close by timeout). Blocking.
 */
void mrt_disconnect(int id) {
  connection_t *conn_p = NULL;
  pthread_mutex_lock(&q_lock);
  conn_p = get_item_q(connections_q, connection_matcher, &id);
  if (conn_p == NULL) {
    pthread_mutex_unlock(&q_lock);
    printf("mrt_disconnect(): spurious call with id=%d.\n", id);
    return; 
  }
  pthread_mutex_unlock(&q_lock);

  // only proceed if no more data buffered...!
  while(1) {
    // get it again to ensure the connection is still valid
    pthread_mutex_lock(&q_lock);
    conn_p = get_item_q(connections_q, connection_matcher, &id);
    if (conn_p == NULL) {
      // the conn_p is NULL now... after not being NULL once...
      pthread_mutex_unlock(&q_lock);
      printf("sender %d: connection dropped before own RCLS gets sent.\n", id);
      // TODO: anyway to tell how many bytes are acknowledged?
      return;  
    }
    pthread_mutex_unlock(&q_lock);

    pthread_mutex_lock(&(conn_p->buffer_lock));
    // HOW CLEVER! IT ALL CAME TOGETHER!
    if (conn_p->last_payload_index < 0) {
      pthread_mutex_unlock(&(conn_p->buffer_lock));
      break;
    }
    pthread_mutex_unlock(&(conn_p->buffer_lock));
    usleep(MRT_DISCONNECT_PERIOD);
  }

  // NOW send RCLS...
  pthread_mutex_lock(&(conn_p->outgoing_lock));
  build_rcls(conn_p->outgoing_buffer);
  sendto(conn_p->send_sockfd, conn_p->outgoing_buffer, 
              MRT_HASH_LENGTH + MRT_TYPE_LENGTH,  
              0, (const struct sockaddr *)(&(conn_p->rece_addr)), 
              addr_len);
  pthread_mutex_unlock(&(conn_p->outgoing_lock));
  
  /* TODO: do nothing here, maybe? Since handler also does this
   * trick when receiving an ACLS...
   */
  pthread_mutex_lock(&(conn_p->timeout_lock));
  conn_p->inactive_time = CLOSE_TIMEOUT_THRESHOLD + 1;
  pthread_mutex_unlock(&(conn_p->timeout_lock));
}

/****** thread functions (unavailable to module users) ******/

/* The main handler; all incoming transmissions will be validated
 * and handled here.
 *
 * deletes the connections_q if the last sender exited (should not affect
 * anything - handler also sets the pointer to be NULL so it will be
 * re-initialized in the next mrt_onnect())
 */
void *handler(void *conn_vp) {
  connection_t *conn_p = (connection_t *)conn_vp;
  int num_bytes_received = 0;
  struct sockaddr_in addr_holder = {0}; // to be used in recvfrom() only
  unsigned long hash_holder = 0;
  unsigned int addr_len_holder = addr_len; // MUST BE addr_len... semantically...
  int type_holder = 0, frag_holder = 0, winsize_holder = 0;
  int frag_difference = 0, num_remaining_bytes = 0;
  char *remaining_bytes_location = NULL;

  // the main loop; handle all the incoming transmissions
  while (1) {
    num_bytes_received = recvfrom(conn_p->send_sockfd, conn_p->incoming_buffer,
                      MRT_HEADER_LENGTH, 0, (struct sockaddr *)(&addr_holder),
                      &addr_len_holder);
    
    // before processing, check if close is flagged
    pthread_mutex_lock(&(conn_p->close_lock));
    if (conn_p->should_close == 1) {
      pthread_mutex_unlock(&conn_p->close_lock);
      printf("sender %d: breaking from handler loop because should_close\n", conn_p->id);
      break;
    }
    pthread_mutex_unlock(&(conn_p->close_lock));

    // NULL-terminate the transmission to enable hash()
    conn_p->incoming_buffer[num_bytes_received] = '\0';

    // first validate the transmission with checksum
    memmove(&hash_holder, conn_p->incoming_buffer, MRT_HASH_LENGTH);

    if (hash(conn_p->incoming_buffer + MRT_HASH_LENGTH) != hash_holder) {
      continue;
    }

    // then check the transmission type and act accordingly
    memmove(&type_holder, conn_p->incoming_buffer + MRT_TYPE_LOCATION, MRT_TYPE_LENGTH);
    memmove(&frag_holder, conn_p->incoming_buffer + MRT_FRAGMENT_LOCATION, MRT_FRAGMENT_LENGTH);
    memmove(&winsize_holder, conn_p->incoming_buffer + MRT_WINDOWSIZE_LOCATION, MRT_WINDOWSIZE_LENGTH);

    switch (type_holder) {
      
      case MRT_ACON :
        // start the sender_thread if it hasn't yet (meaning first ACON)
        // TODO: what if pthread_create() fails?
        pthread_mutex_lock(&(conn_p->receiver_lock));
        if (conn_p->last_acknowledged_frag == -1) {
          pthread_create(&(conn_p->sender_thread), NULL, sender, conn_p);
          pthread_create(&(conn_p->checker_thread), NULL, checker, conn_p);
          conn_p->last_acknowledged_frag = 0;
        }
        pthread_mutex_unlock(&(conn_p->receiver_lock));
        // otherwise do nothing (duplicate ACONs are ignored)
        break;

      case MRT_ADAT :
        // first of all, receiver just proved the connection is alive
        pthread_mutex_lock(&(conn_p->timeout_lock));
        conn_p->inactive_time = 0;
        pthread_mutex_unlock(&(conn_p->timeout_lock));
        
        /* if the ADAT is at least as new as the last one, update
         * the frag and receiver_window_size if necessary
         */
        pthread_mutex_lock(&(conn_p->receiver_lock));
        frag_difference = frag_holder - conn_p->last_acknowledged_frag;
        if (frag_difference >= 0) {
          conn_p->last_acknowledged_frag = frag_holder;
          if (conn_p->receiver_window_size < winsize_holder) {
            conn_p->receiver_window_size = winsize_holder;
          }
        }
        // if we can free up the buffer, do it
        // note that we cannot release receiver_lock yet!
        if (frag_difference > 0) {
          pthread_mutex_lock(&(conn_p->buffer_lock));
          // update the buffer
          remaining_bytes_location = conn_p->sender_buffer + MAX_MRT_PAYLOAD_LENGTH * frag_difference;
          num_remaining_bytes = MAX_MRT_PAYLOAD_LENGTH * (conn_p->last_payload_index - frag_difference + 1);
          memmove(conn_p->sender_buffer, remaining_bytes_location, num_remaining_bytes);
          // update the num_bytes_buffered array
          remaining_bytes_location = (char *)(conn_p->num_bytes_buffered + frag_difference);
          num_remaining_bytes = sizeof(int) * (conn_p->last_payload_index - frag_difference + 1);
          memmove(conn_p->num_bytes_buffered, remaining_bytes_location, num_remaining_bytes);
          // update the last_payload_index
          conn_p->last_payload_index -= frag_difference;
          pthread_mutex_unlock(&(conn_p->buffer_lock));
        }
        pthread_mutex_unlock(&(conn_p->receiver_lock));
        break;

      case MRT_ACLS :
        // could just do nothing here but...
        pthread_mutex_lock(&(conn_p->timeout_lock));
        conn_p->inactive_time = CLOSE_TIMEOUT_THRESHOLD + 1;
        pthread_mutex_unlock(&(conn_p->timeout_lock));
        break;

      default :
        // RCON, DATA, RCLS, UNKN
        continue;
    }
  }
  /* Do the clean-ups
   */
  int id = conn_p->id;
  printf("sender %d: closing. Cleaning up.\n", id);
  pthread_join(conn_p->checker_thread, NULL);
  pthread_join(conn_p->sender_thread, NULL);

  pthread_mutex_lock(&q_lock);
  connection_t_free(pop_item_q(connections_q, connection_matcher, &id));
  if (peek_q(connections_q) == NULL) { 
    delete_q(connections_q, connection_t_free); 
    connections_q = NULL;
  }
  pthread_mutex_unlock(&q_lock);
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
  connection_t *conn_p = (connection_t *)conn_vp;
  int resend_time = 0;

  while (1) {
    pthread_mutex_lock(&(conn_p->close_lock));
    if (conn_p->should_close == 1) {
      pthread_mutex_unlock(&(conn_p->close_lock));
      break;
    }
    pthread_mutex_unlock(&(conn_p->close_lock));
    
    // TODO: simplify dangerously nested mutex
    pthread_mutex_lock(&(conn_p->receiver_lock));
    pthread_mutex_lock(&(conn_p->buffer_lock));
    int next_payload_index = conn_p->last_sent_index + 1;
    if (conn_p->receiver_window_size < MAX_MRT_PAYLOAD_LENGTH
        || next_payload_index > conn_p->last_payload_index) {
      if (next_payload_index > conn_p->last_payload_index) {
        // the sender has nothing to send, consider resending fragments
        if (resend_time > RESEND_TIMEOUT_THRESHOLD) {
          conn_p->last_sent_index = -1;
          resend_time = 0;
        } else { resend_time += EMPTY_DATA_PERIOD; }
      } else {
        // the sender has something to send; reset timer
        resend_time = 0; 
      }
      pthread_mutex_unlock(&(conn_p->buffer_lock));
      pthread_mutex_unlock(&(conn_p->receiver_lock));

      // send empty DATA and sleep
      pthread_mutex_lock(&(conn_p->outgoing_lock));
      build_data_empty(conn_p->outgoing_buffer);
      sendto(conn_p->send_sockfd, conn_p->outgoing_buffer, 
              MRT_HASH_LENGTH + MRT_TYPE_LENGTH + MRT_FRAGMENT_LENGTH,  
              0, (const struct sockaddr *)(&(conn_p->rece_addr)), 
              addr_len);
      pthread_mutex_unlock(&(conn_p->outgoing_lock));
      usleep(EMPTY_DATA_PERIOD);
      continue; // just to be safe
    } else {
      // the sender has something to send; reset timer
      resend_time = 0;
      // send meaningful DATA
      pthread_mutex_lock(&(conn_p->outgoing_lock));
      int payload_length = (conn_p->num_bytes_buffered)[next_payload_index];
      build_data(conn_p, next_payload_index, payload_length);
      sendto(conn_p->send_sockfd, conn_p->outgoing_buffer, 
              MRT_PAYLOAD_LOCATION + payload_length, 0,
              (const struct sockaddr *)(&(conn_p->rece_addr)), 
              addr_len);
      pthread_mutex_unlock(&(conn_p->outgoing_lock));
      conn_p->last_sent_index += 1;
      pthread_mutex_unlock(&(conn_p->buffer_lock));
      pthread_mutex_unlock(&(conn_p->receiver_lock));
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
  connection_t *conn_p = (connection_t *)conn_vp;
  while (1) {
    pthread_mutex_lock(&(conn_p->timeout_lock));
    conn_p->inactive_time += CLOSE_TIMEOUT_INCREMENT;
    // if it would sleep past the threshold, go BOOM
    if (conn_p->inactive_time > CLOSE_TIMEOUT_THRESHOLD) {
      pthread_mutex_unlock(&(conn_p->timeout_lock));
      pthread_mutex_lock(&(conn_p->close_lock));
      conn_p->should_close = 1;
      pthread_mutex_unlock(&(conn_p->close_lock));
      break;
    }
    pthread_mutex_unlock(&(conn_p->timeout_lock));
    usleep(CLOSE_TIMEOUT_INCREMENT);
    continue; // just to be safe
  }
  // TODO: any garbage collection necessary?
  return NULL;
}

/****** helper functions (unavailable to module users) ******/

/* initialize a new connection struct and returns its pointer
 * the caller is responsible for freeing it.
 */
connection_t *connection_t_init(unsigned short sender_port_number, unsigned short receiver_port_number, unsigned long receiver_s_addr) {
  connection_t *connection_p = calloc(1, sizeof(connection_t));

  if (connection_p == NULL) { return NULL; }

  if (pthread_mutex_init(&(connection_p->buffer_lock), NULL) != 0 ||
      pthread_mutex_init(&(connection_p->receiver_lock), NULL) != 0 ||
      pthread_mutex_init(&(connection_p->timeout_lock), NULL) != 0 ||
      pthread_mutex_init(&(connection_p->close_lock), NULL) != 0 ||
      pthread_mutex_init(&(connection_p->outgoing_lock), NULL) != 0
      ) { return NULL; }

  connection_p->send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (connection_p->send_sockfd < 0) { return NULL; }

  connection_p->send_addr.sin_family = AF_INET;
  connection_p->send_addr.sin_port = htons(sender_port_number);
  connection_p->send_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  
  // Yup... it's gonna be listenin', too. I forgot about this.
  if (bind(connection_p->send_sockfd, (struct sockaddr *)&(connection_p->send_addr), addr_len) < 0) {
    perror("bind(connection_p->send_sockfd) error\n");
    return NULL;
  }

  connection_p->id = next_id++;

  connection_p->rece_addr.sin_family = AF_INET;
  connection_p->rece_addr.sin_port = htons(receiver_port_number);
  connection_p->rece_addr.sin_addr.s_addr = htonl(receiver_s_addr);

  connection_p->last_sent_index = -1;
  connection_p->last_payload_index = -1;
  
  connection_p->receiver_window_size = 0;
  connection_p->last_acknowledged_frag = -1;

  connection_p->inactive_time = 0;

  connection_p->should_close = 0;

  return connection_p;
}

/* the clean-up function to be called before handler() returns
 * signature is so that it can be used as a clean-up callback to q
 */
void connection_t_free(void *conn_vp) {
  connection_t *conn_p = (connection_t *)conn_vp;
  // just in case
  if (conn_p == NULL) { return; }

  close(conn_p->send_sockfd);

  pthread_mutex_destroy(&(conn_p->buffer_lock));
  pthread_mutex_destroy(&(conn_p->receiver_lock));
  pthread_mutex_destroy(&(conn_p->timeout_lock));
  pthread_mutex_destroy(&(conn_p->close_lock));
  pthread_mutex_destroy(&(conn_p->outgoing_lock));

  free(conn_p);
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

void build_data(connection_t *conn_p, int payload_index, int payload_len) {
  int last_acknowledged_frag = conn_p->last_acknowledged_frag;
  char *sender_buffer = conn_p->sender_buffer;
  char *outgoing_buffer = conn_p->outgoing_buffer;

  int sending_frag = last_acknowledged_frag + payload_index + 1;

  memmove(outgoing_buffer + MRT_TYPE_LOCATION, &data_type, MRT_TYPE_LENGTH);
  memmove(outgoing_buffer + MRT_FRAGMENT_LOCATION, &sending_frag, MRT_FRAGMENT_LENGTH);
  memmove(outgoing_buffer + MRT_PAYLOAD_LOCATION, sender_buffer + payload_index * MAX_MRT_PAYLOAD_LENGTH, payload_len);

  outgoing_buffer[MRT_PAYLOAD_LOCATION + payload_len] = '\0';
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

