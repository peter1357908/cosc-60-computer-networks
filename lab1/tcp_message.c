/* Original code on https://www.geeksforgeeks.org/socket-programming-cc/
 * Modified by Shengsong Gao for COSC 60 Lab 1
 *
 * Send a message containing my name with TCP to localhost:8099
 */

#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string.h> 
#define PORT 8099 

int main(int argc, char const *argv[]) 
{ 
	int sock = 0; 
	struct sockaddr_in serv_addr; 
	char *message = "Shengsong Gao\n"; 
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{ 
		printf("\n Socket creation error \n"); 
		return -1; 
	} 

	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(PORT); 
	
	// Convert IPv4 and IPv6 addresses from text to binary form 
	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) 
	{ 
		printf("\nInvalid address/ Address not supported \n"); 
		return -1; 
	} 

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
	{ 
		printf("\nConnection Failed \n"); 
		return -1; 
	} 
	send(sock, message, strlen(message), 0);
	return 0; 
} 
