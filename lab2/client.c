/* The client implementation for a multi-client TCP local chatroom
 * employing the custom-made Dual-Purpose Protocol.
 *
 * command line:
 *	client server_port username
 *
 * `username` shall not contain DPP's delimiter
 *
 *
 * For Dartmouth COSC 60 Lab 2;
 * By Shengsong Gao, 2020.
 */

#include <stdio.h>
#include <stdlib.h> // exit()
#include <sys/socket.h> 
#include <arpa/inet.h> // htons()
#include <pthread.h>
#include <string.h>
#include "dpp.h"

#define MAX_MESSAGE_LENGTH 512
#define MAX_USERNAME_LENGTH 20
#define BUFFER_SIZE (MAX_MESSAGE_LENGTH + MAX_USERNAME_LENGTH + DPP_MAX_OVERHEAD + 1)

pthread_mutex_t join_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t join_cvar = PTHREAD_COND_INITIALIZER;
int join_acknowledged = 0; // 0: not acknowledged; 1: acknowledged.

// TODO: is synchronization for quit flag really necessary?
pthread_mutex_t quit_lock = PTHREAD_MUTEX_INITIALIZER;
int quit_necessary = 0; // 0: not necessary; 1: necessary

// just a bit of UI sugar (tells the user to get out of the sender loop)
pthread_mutex_t terminate_lock = PTHREAD_MUTEX_INITIALIZER;
int sender_thread_terminated = 0; // 0: sender still running; 1: sender terminated

char const *username;
int sockfd;
void *client_sender(void *);

int main(int argc, char const *argv[]) 
{
	/****** parsing arguments ******/
	if (argc != 3) {
		fprintf(stderr, "usage: %s server_port username\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	username = argv[2];
	if (strlen(username) > MAX_USERNAME_LENGTH) {
		fprintf(stderr, "Username too long! Max characters allowed: %d\n", MAX_USERNAME_LENGTH);
		exit(EXIT_FAILURE);
	}
	if (strstr(username, DPP_DELIMITER) != NULL) {
		fprintf(stderr, "Username shall not contain the DPP delimiter \"%s\"\n", DPP_DELIMITER);
		exit(EXIT_FAILURE);
	}
	unsigned short serv_port = (unsigned short) atoi(argv[1]);
	
	/****** initializing socket and server address ******/
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("socket() error\n");
		exit(EXIT_FAILURE);
	}
	
	struct sockaddr_in serv_addr = {0};
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(serv_port);
	serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	
	/****** establishing connection ******/
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { 
		perror("connect() error\n");
		exit(EXIT_FAILURE);
	}
	
	/****** interactive session ******/
	
	// TODO: modularize sending and receiving
	pthread_t sender_thread;
	if (pthread_create(&sender_thread, NULL, client_sender, NULL) != 0) {
		perror("pthread_create() error\n");
		exit(EXIT_FAILURE);
	}
	
	char receiver_buffer[BUFFER_SIZE];
	char *received_username, *received_message;
	int transmission_type, num_char_received;
	
	// main receiving loop
	while (1) {
		num_char_received = recv(sockfd, receiver_buffer, BUFFER_SIZE, 0);
		if (num_char_received < 0) {
			perror("recv() error\n");
			exit(EXIT_FAILURE);
		} else if (num_char_received == 0) {
			pthread_mutex_lock(&quit_lock);
			printf("[Connection terminated; preparing to quit...]\n");
			quit_necessary = 1;
			pthread_mutex_unlock(&quit_lock);
			break;
		}
		
		transmission_type = parse_transmission(receiver_buffer, &received_username, &received_message, num_char_received);
		
		if (transmission_type == DPP_MESSAGE_TYPE) {
			printf("<%s>: %s\n", received_username, received_message);
			
		} else if (transmission_type == DPP_JOIN_TYPE) {
			if (strcmp(received_username, username) == 0) {
				pthread_mutex_lock(&join_lock);
				printf("[Successfully joined chatroom!]\n");
				join_acknowledged = 1;
				pthread_cond_signal(&join_cvar);
				pthread_mutex_unlock(&join_lock);
			} else {
				printf("[<%s> joined chatroom!]\n", received_username);
			}			
		} else if (transmission_type == DPP_QUIT_TYPE) {
			if (strcmp(received_username, username) == 0) {
				pthread_mutex_lock(&quit_lock);
				printf("[Gracefully disconnected; preparing to quit...]\n");
				quit_necessary = 1;
				pthread_mutex_unlock(&quit_lock);
				break;
			}
			printf("[<%s> quitted chatroom!]\n", received_username);
		} else if (transmission_type == DPP_UNKNOWN_TYPE) {
			printf("[Garbo transmission received from server...]\n");
		} else {
			printf("[Garbo return value from parsing... HOW\?!]\n");
		}
	}
	
	// if the sender is still blocked in fgets()...
	pthread_mutex_lock(&terminate_lock);
	if (sender_thread_terminated == 0) {
		printf("[Enter anything to quit: ]");
	}
	pthread_mutex_unlock(&terminate_lock);
	
	pthread_join(sender_thread, NULL);
	printf("[Quitting for real.]\n");
	
	return 0;
}

void *client_sender(void *this_argument_is_useless) {
	char sender_buffer[BUFFER_SIZE], reading_buffer[MAX_MESSAGE_LENGTH + 1];

	// first, try to join the chatroom
	printf("[Trying to join the chatroom...]\n");
	if (build_join(sender_buffer, username) < 0) {
		perror("build_join() error\n");
		exit(EXIT_FAILURE);
	}
	if (send(sockfd, sender_buffer, strlen(sender_buffer), 0) < 0) {
		perror("send() error during joining\n");
		exit(EXIT_FAILURE);
	}
	
	// only allow chatting after join request acknowledged
	// TODO: should we check for quit_necessary inside?
	pthread_mutex_lock(&join_lock);
	while (join_acknowledged == 0) {
		pthread_cond_wait(&join_cvar, &join_lock);
	}
	pthread_mutex_unlock(&join_lock);
	
	// main sending loop
	while (1) {
		pthread_mutex_lock(&quit_lock);
		if (quit_necessary == 1) {
			// TODO: will "break;" result in the lock being released?
			pthread_mutex_unlock(&quit_lock);
			break;
		}
		pthread_mutex_unlock(&quit_lock);
		
		// TODO: improve the UI/reading logic
		if (fgets(reading_buffer, MAX_MESSAGE_LENGTH + 1, stdin) == NULL) {
			// encountered EOF, interpret as quitting
			pthread_mutex_lock(&quit_lock);
			printf("[Disconnection requested!]\n");
			quit_necessary = 1;
			pthread_mutex_unlock(&quit_lock);
			build_quit(sender_buffer, username);
		} else {
			build_message(sender_buffer, username, reading_buffer);
		}
		
		if (send(sockfd, sender_buffer, strlen(sender_buffer), 0) < 0) {
			perror("send() error during main sending loop\n");
			exit(EXIT_FAILURE);
		}
	}
	
	pthread_mutex_lock(&terminate_lock);
	sender_thread_terminated = 1;
	pthread_mutex_unlock(&terminate_lock);
	
	return NULL;
}


