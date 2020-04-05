/* Original code on https://www.geeksforgeeks.org/udp-server-client-implementation-c/
 * Modified by Shengsong Gao for COSC 60 Lab 1
 *
 * Send a message containing my name with UDP to localhost:2020
 */
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 

#define PORT	2020 

// Driver code 
int main() { 
	int sockfd; 
	char *message = "Shengsong Gao\n"; 
	struct sockaddr_in serv_addr; 

	// Creating socket file descriptor 
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
		perror("socket creation failed"); 
		exit(EXIT_FAILURE); 
	} 

	memset(&serv_addr, 0, sizeof(serv_addr)); 
	
	// Filling server information 
	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(PORT); 
		
	// Convert IPv4 and IPv6 addresses from text to binary form 
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)  
    { 
        printf("\nInvalid address/ Address not supported \n"); 
        return -1; 
    } 
	
	int sendtorc;
	sendtorc = sendto(sockfd, (const char *) message, strlen(message), 
		0, (const struct sockaddr *) &serv_addr, 
			sizeof(serv_addr));
	if (sendtorc < 0) {
		perror("sendto failed");
		exit(1);
	} 
	
	close(sockfd); 
	return 0; 
} 
