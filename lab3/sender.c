/* The sender application testing the mrt_sender module
 *
 * command line:
 *	sender sender_port_number read_size
 *
 * For Dartmouth COSC 60 Lab 3;
 * By Shengsong Gao, May 2020.
 */

#include <stdio.h>
#include <stdlib.h> // atoi()
#include <unistd.h> // read(), STDIN_FILENO
#include <netinet/in.h>  // INADDR_LOOPBACK
#include "mrt_sender.h"

#define RECEIVER_PORT_NUMBER 7878
#define BUFFER_SIZE 1000

int main(int argc, char const *argv[]) {
  /****** parsing arguments ******/
	if (argc != 3) {
		fprintf(stderr, "usage: %s sender_port_number read_size\n", argv[0]);
		return -1;
	}
	unsigned short sender_port_number = (unsigned short)(atoi(argv[1]));
  int read_size = atoi(argv[2]);

  int id = mrt_connect(sender_port_number, RECEIVER_PORT_NUMBER, INADDR_LOOPBACK);

  if (id < 0) {
    perror("mrt_connect() failed...\n");
    return -1;
  }

  char buffer[BUFFER_SIZE] = {0};
  int num_bytes_read = 0;

  while (1) {
    num_bytes_read = read(STDIN_FILENO, buffer, read_size);
    if (num_bytes_read <= 0) {
      break;
    } else {
      mrt_send(id, buffer, num_bytes_read);
    }
  }

  mrt_disconnect(id);

  return 0;
}
