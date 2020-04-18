/* The server implementation for a multi-client TCP local chatroom
 *
 * command line:
 *	server server_port
 *
 *
 * For Dartmouth COSC 60 Lab 2;
 * By Shengsong Gao, 2020.
 */

#include <stdio.h>
#include <stdlib.h> // exit(), malloc(), free()
#include <unistd.h> // close()
#include <sys/socket.h> 
#include <arpa/inet.h> // htons()
#include <pthread.h>
#include <string.h>
#include "dpp.h"
#include "Queue.h"

// TODO: share the definitions between server and client...
#define MAX_MESSAGE_LENGTH 512
#define MAX_USERNAME_LENGTH 20
#define BUFFER_SIZE (MAX_MESSAGE_LENGTH + MAX_USERNAME_LENGTH + DPP_MAX_OVERHEAD + 1)

void *server_receiver(void *);
void broadcast_callback(void *);

pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;
q_t *server_receiver_thread_q;
q_t *accepted_sockfd_q;
char broadcast_buffer[BUFFER_SIZE] = {0};
int broadcast_length;

int main(int argc, char const *argv[]) 
{
	/****** parsing arguments ******/
	if (argc != 2) {
		fprintf(stderr, "usage: %s server_port\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	unsigned short serv_port = (unsigned short) atoi(argv[1]);

	/****** initializing socket and server address ******/
	int serv_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (serv_sockfd < 0) {
		perror("serv_sockfd = socket() error\n");
		exit(EXIT_FAILURE);
	}
	
	struct sockaddr_in serv_addr = {0};
	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(serv_port); 
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	/****** listening for connections ******/
	if (bind(serv_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("bind(serv_sockfd) error\n"); 
		exit(EXIT_FAILURE);
	}
	if (listen(serv_sockfd, 100) < 0) { 
		perror("listen"); 
		exit(EXIT_FAILURE);
	}
	
	/****** multi-thread operations ******/
	server_receiver_thread_q = make_q();
	accepted_sockfd_q = make_q();
	pthread_t *new_thread_p;
	int *new_accepted_sockfd_p;
	
	unsigned int addr_len = (unsigned int) sizeof(serv_addr);
	
	while (1) {
		/****** sync'd: accept and create new thread ******/
		pthread_mutex_lock(&q_lock);
		
		new_accepted_sockfd_p = (int *)malloc(sizeof(int));
		if ((*new_accepted_sockfd_p = accept(serv_sockfd, (struct sockaddr *)&serv_addr, &addr_len)) < 0) {
			perror("accept() error\n"); 
			exit(EXIT_FAILURE);
		}
		enq_q(accepted_sockfd_q, new_accepted_sockfd_p);
		
		new_thread_p = (pthread_t *)malloc(sizeof(pthread_t));
		if (pthread_create(new_thread_p, NULL, server_receiver, new_accepted_sockfd_p) != 0) {
			perror("pthread_create() error\n");
			exit(EXIT_FAILURE);
		}
		enq_q(server_receiver_thread_q, new_thread_p);
		
		pthread_mutex_unlock(&q_lock);
	}
	
	// TODO: enable server to quit gracefully (free queues, join threads, etc.)

	return 0; 
} 

void *server_receiver(void *accepted_sockfd_vp) {
	char receiver_buffer[BUFFER_SIZE] = {0};
	int num_char_received;
	
	int accepted_sockfd = *((int *) accepted_sockfd_vp);
	
	while (1) {
		num_char_received = recv(accepted_sockfd, receiver_buffer, BUFFER_SIZE, 0);
		// if received anything, just mindlessly broadcast them first
		if (num_char_received > 0) {
			printf("num_char_received = %d\n"
					"receiver_buffer = %.9s\n", num_char_received, receiver_buffer);
			pthread_mutex_lock(&q_lock);
			memmove(broadcast_buffer, receiver_buffer, num_char_received);
			printf("memmove() finished, broadcast_buffer = %.9s\n", broadcast_buffer);
			broadcast_length = num_char_received;
			printf("length:%d\n", broadcast_length);
			
			iterate_q(accepted_sockfd_q, broadcast_callback);
			pthread_mutex_unlock(&q_lock);
			
			// if the client doesn't want to quit yet, just continue...
			if (is_quit(receiver_buffer) != 1) {
				continue;
			}
		}
		
		/* if this part is reached, then any of the following happened:
		 * 1. recv() encountered error;
		 * 2. recv() indicates that a disconnection happened;
		 * 3. the client requested to quit;
		 */
		shutdown(accepted_sockfd, 2);
		close(accepted_sockfd);
		pthread_mutex_lock(&q_lock);
		delete_integer_targets_q(accepted_sockfd_q, accepted_sockfd);
		pthread_mutex_unlock(&q_lock);
		break;
	}
	
	return NULL;
}

void broadcast_callback(void *accepted_sockfd_vp) {
	// TODO: take care of potential send() error...
	int accepted_sockfd = *((int *) accepted_sockfd_vp);
	send(accepted_sockfd, broadcast_buffer, broadcast_length, 0);
}
