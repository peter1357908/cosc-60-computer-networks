/* Sender functions for the Mini Reliable Transport module.
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
#include "mrt_sender.h"
#include "Queue.h"

#define PORT_NUMBER 4242

int mrt_open() {
  /****** initializing socket and server address ******/
  int serv_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (serv_sockfd < 0)
  {
    perror("serv_sockfd = socket() error\n");
    return MRT_ERROR;
  }

  struct sockaddr_in serv_addr = {0};
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT_NUMBER);
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  /****** binding the socket ******/
  if (bind(serv_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("bind(serv_sockfd) error\n");
    return MRT_ERROR;
  }

  return MRT_SUCCESS;
}


 
 