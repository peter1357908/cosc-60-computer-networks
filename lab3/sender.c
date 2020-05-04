/* The sender application testing the mrt_sender module
 *
 * command line:
 *	sender
 *
 * For Dartmouth COSC 60 Lab 3;
 * By Shengsong Gao, May 2020.
 */

#include <stdio.h>
#include <string.h>
#include <netinet/in.h>  // INADDR_LOOPBACK
#include "mrt_sender.h"

#define PORT_NUMBER 4242
#define BUFFER_SIZE 1000

int main(int argc, char const *argv[]) {
  int id = mrt_connect(PORT_NUMBER, INADDR_LOOPBACK);

  if (id < 0) {
    perror("mrt_connect() failed...\n");
    return -1;
  }

  char buffer[BUFFER_SIZE];

  while (1) {
    // the actual number of chars read: (BUFFER_SIZE - 1)
    if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
      // encountered EOF, interpret as end of data
      printf("[EOF received; ending data collection...]\n");
      break;
    } else {
      mrt_send(id, buffer, strlen(buffer));
    }
  }

  mrt_disconnect(id);

  return 0;
}
