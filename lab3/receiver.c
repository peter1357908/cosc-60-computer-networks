/* The receiver application testing the mrt_receiver module
 *
 * command line:
 *	receiver num_connections
 *
 * For Dartmouth COSC 60 Lab 3;
 * By Shengsong Gao, May 2020.
 */

#include <stdio.h>
#include <stdlib.h> // atoi(), free()
#include <string.h>
#include <sys/socket.h>  // (struct sockaddr_in)
#include "Queue.h"
#include "mrt_receiver.h"

#define PORT_NUMBER 4242
#define BUFFER_SIZE 1000

int main(int argc, char const *argv[]) {
  /****** parsing arguments ******/
	if (argc != 2) {
		fprintf(stderr, "usage: %s num_connections\n", argv[0]);
		return -1;
	}
	int num_connections = atoi(argv[1]);

  /****** initialization (data structures and connection) ******/
  q_t *sender_id_q = make_q();
  if (sender_id_q == NULL) {
    perror("make_q() error... \n");
    return -1;
  }

  if (mrt_open(PORT_NUMBER) < 0) {
    perror("mrt_open() error...\n");
    return -1;
  }

  /* after all senders are connected, for each sender in order
   * of connection, output its sent data until it disconnects
   */

  struct sockaddr_in *curr_sender_id = NULL;
  char buffer[BUFFER_SIZE + 1]; // +1 for '\0'
  int i, num_bytes_read;

  for (i = 0; i < num_connections; i++) {
    enq_q(sender_id_q, mrt_accept1());
  }

  for (i = 0; i < num_connections; i++) {
    curr_sender_id = deq_q(sender_id_q);
    
    while ((num_bytes_read = mrt_receive1(curr_sender_id, buffer, BUFFER_SIZE)) > 0) {
      buffer[num_bytes_read] = '\0';
      printf("%s", buffer);
    }
    
    free(curr_sender_id);
  }

  mrt_close();
  return 0;
}
