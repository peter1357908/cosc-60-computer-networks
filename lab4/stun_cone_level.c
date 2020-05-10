/* STUN client for the STUN NAT-detector module
 * tests the level of the user's NAT cone (if it's not symmetric)
 *
 * Completely reliant on `77.72.169.210:3478` being alive.
 *
 * If no messages are printed, the user is behind a Port Restricted NAT
 *	
 * For Dartmouth COSC 60 Lab 4;
 * By Shengsong Gao, May 2020.
 */

#define _POSIX_C_SOURCE 200112L // for getaddrinfo() and the like

#include <stdio.h>
#include <string.h>
#include <unistd.h> // close()
#include <stdint.h> // uint16_t and uint32_t
#include <sys/socket.h>
#include <sys/types.h> // getaddrinfo()
#include <netdb.h> // getaddrinfo(), getnameinfo(), ...
#include <arpa/inet.h> // htons()
#include <pthread.h>

#define STUN_CLIENT_PORT          7878

#define STUN_HEADER_SIZE          20
#define STUN_CHANGE_ADDRESS_SIZE  8
#define STUN_WITH_CHANGE_SIZE     (STUN_HEADER_SIZE+STUN_CHANGE_ADDRESS_SIZE)
#define STUN_BUFFER_SIZE          (STUN_WITH_CHANGE_SIZE+1000)
#define HOSTNAME_HOLDER_SIZE      100

/****** function declarations ******/
void *response_handler(void *);
void build_port_spoof();
void build_both_spoof();

/****** global variables ******/
/* all requests go to 77.72.169.210:3478:
 * 2 responses potentially with changed port
 * 2 responses potentially with changed port AND ip
 */
int num_response_expected = 4;
unsigned int addr_len = (unsigned int) sizeof(struct sockaddr_in);
int client_sockfd;
char request_buffer[STUN_BUFFER_SIZE] = {0};
char *original_server_ipv4 = "77.72.169.210";
unsigned short original_server_port = 3478;

/****** main ******/
int main() {
  // open up a socket
  if ((client_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
    perror("client_sockfd creation failed\n"); 
    return -1;
  }

  // bind and start listening before sending requests
  struct sockaddr_in client_addr = {0};
  client_addr.sin_family = AF_INET;
  client_addr.sin_port = htons(STUN_CLIENT_PORT);
  client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(client_sockfd, (const struct sockaddr *)&client_addr, addr_len) < 0) { 
    perror("bind failed\n"); 
    return -1;
  }

  pthread_t response_handler_thread = {0};
  if (pthread_create(&response_handler_thread, NULL, response_handler, NULL) != 0) {
    perror("pthread_create(response_handler) error\n");
    return -1;
  }

  // build the STUN server address
  struct sockaddr_in stun_server_addr = {0};
  stun_server_addr.sin_family = AF_INET;
  if (inet_pton(AF_INET, original_server_ipv4, &(stun_server_addr.sin_addr)) != 1) {
    perror("inet_pton() failed\n");
    return -1;
  }
  stun_server_addr.sin_port = htons(original_server_port);

  /* TODO: write code that expect the changed port by sending an
   * attribute-less binding request first.
   *
   * TODO: make use of both pairs: 
   * 77.72.169.(210/211):(3478/3479)
   * 77.72.169.(212/213):(3478/3479)
   */
  
  // 2 requests for changed port only
  build_port_spoof();
  sendto(client_sockfd, request_buffer, STUN_WITH_CHANGE_SIZE,
            0, (const struct sockaddr *)(&stun_server_addr), 
            addr_len);
  sendto(client_sockfd, request_buffer, STUN_WITH_CHANGE_SIZE,
            0, (const struct sockaddr *)(&stun_server_addr), 
            addr_len);
  
  // 2 requests for changed ip and changed port
  build_both_spoof();
  sendto(client_sockfd, request_buffer, STUN_WITH_CHANGE_SIZE,
            0, (const struct sockaddr *)(&stun_server_addr), 
            addr_len);
  sendto(client_sockfd, request_buffer, STUN_WITH_CHANGE_SIZE,
            0, (const struct sockaddr *)(&stun_server_addr), 
            addr_len);

  pthread_join(response_handler_thread, NULL);
  close(client_sockfd);

  return 0;
}

// assumes that the received messages are all STUN messages
void *response_handler(void *_nil) {
  char response_buffer[STUN_BUFFER_SIZE] = {0}; // in network byte order
  unsigned int addr_len_holder = addr_len;
  char server_hostname_holder[HOSTNAME_HOLDER_SIZE] = {0};
  char attr_hostname_holder[HOSTNAME_HOLDER_SIZE] = {0}; // holds IPv4 addresses
  
  struct sockaddr_in server_addr_holder = {0};
  struct sockaddr_in attr_addr_holder = {0};
  attr_addr_holder.sin_family = AF_INET; // getnameinfo() needs this filled

  for (int i = 0; i < num_response_expected; i++) {
    int num_bytes_received = recvfrom(client_sockfd, response_buffer,
                      STUN_BUFFER_SIZE, 0, (struct sockaddr *)(&server_addr_holder),
                      &addr_len_holder);

    getnameinfo((struct sockaddr *)(&server_addr_holder), addr_len, server_hostname_holder, HOSTNAME_HOLDER_SIZE, NULL, 0, NI_NUMERICHOST);
    printf("%s responded:\n", server_hostname_holder);

    if (num_bytes_received > 0) {
      uint16_t num_bytes_attributes = ntohs(((uint16_t *)response_buffer)[1]);
      char *response_attribute_location = response_buffer + STUN_HEADER_SIZE;
      // printf("Attributes portion has %hu bytes\n", num_bytes_attributes);

      if (((uint16_t *)response_buffer)[0] == htons(0x0101)) {
        // Binding Response (success); assumes that own address is IPv4
        // puts("\'Binding Response\' (SUCCESS) received, start iterating through attributes");
        uint16_t index = 0;
        while (index < num_bytes_attributes) {
          response_attribute_location += index;
          uint16_t attribute_length = ntohs(((uint16_t *)response_attribute_location)[1]);
          // printf("attribute_length=%hu\n", attribute_length);
          uint16_t attribute_type_network_byte = ((uint16_t *)response_attribute_location)[0];
          if (attribute_type_network_byte == htons(0x0001)) {
            // attr_addr_holder here means `nat_addr_holder` or `mapped_addr_holder`
            attr_addr_holder.sin_port = ((uint16_t *)response_attribute_location)[3];
            attr_addr_holder.sin_addr.s_addr = ((uint32_t *)response_attribute_location)[2];
            getnameinfo((struct sockaddr *)(&attr_addr_holder), addr_len, attr_hostname_holder, HOSTNAME_HOLDER_SIZE, NULL, 0, NI_NUMERICHOST);
            printf("FOUND IPv4 MAPPED-ADDRESS:\nNAT IP: %s, NAT PORT: %hu\n", attr_hostname_holder, ntohs(attr_addr_holder.sin_port));

            // assumes that each message has at most one MAPPED-ADDRESS attribute
            // check if the port number is different than the original
            if (htons(original_server_port) != server_addr_holder.sin_port) {
              // if the IP number is also different...
              if (strncmp(server_hostname_holder, original_server_ipv4, INET_ADDRSTRLEN) != 0) {
                printf("\nCongratulations! You are behind a Full Cone NAT!\n"
                      "If this socket is kept open, your peers can reach you at\n"
                      "this address:\n"
                      "\n%s:%hu\n",
                      attr_hostname_holder, ntohs(attr_addr_holder.sin_port));
              }
              // if the IP number is the same...
              else {
                printf("\nHmm. You are at least behind a Restricted Cone NAT,\n"
                      "and definitely not behind a Port Restricted Cone NAT.\n"
                      "If you don\'t see a message indicating that you are behind\n"
                      "a full cone NAT, then you really are behind a Festricted\n" "Cone NAT - under such NAT, your peers cannot find you, but\n"
                      "you can try finding your peers... Your address here is:\n"
                      "\n%s:%hu\n",
                      attr_hostname_holder, ntohs(attr_addr_holder.sin_port));
              }
            }

          } else if (attribute_type_network_byte == htons(0x0004)) {
            // attr_addr_holder here means `source_addr_holder`
            attr_addr_holder.sin_port = ((uint16_t *)response_attribute_location)[3];
            attr_addr_holder.sin_addr.s_addr = ((uint32_t *)response_attribute_location)[2];
            getnameinfo((struct sockaddr *)(&attr_addr_holder), addr_len, attr_hostname_holder, HOSTNAME_HOLDER_SIZE, NULL, 0, NI_NUMERICHOST);
            printf("FOUND IPv4 SOURCE-ADDRESS:\nSOURCE IP: %s, NAT PORT: %hu\n", attr_hostname_holder, ntohs(attr_addr_holder.sin_port));
          } else if (attribute_type_network_byte == htons(0x0005)) {
            // attr_addr_holder here means `source_addr_holder`
            attr_addr_holder.sin_port = ((uint16_t *)response_attribute_location)[3];
            attr_addr_holder.sin_addr.s_addr = ((uint32_t *)response_attribute_location)[2];
            getnameinfo((struct sockaddr *)(&attr_addr_holder), addr_len, attr_hostname_holder, HOSTNAME_HOLDER_SIZE, NULL, 0, NI_NUMERICHOST);
            printf("FOUND IPv4 CHANGED-ADDRESS:\nSOURCE IP: %s, NAT PORT: %hu\n", attr_hostname_holder, ntohs(attr_addr_holder.sin_port));
          } else {
            puts("FOUND irrelevant attribute...");
          }
          index += (attribute_length + 4); // +4 for attribute header length
          // printf("After incrementing, index=%hu\n", index);
        }
      } 
      else if (((uint16_t *)response_buffer)[0] == htons(0x0111)) {
        // Binding Error
        puts("\'Binding Error\' received, what happened?");
        // TODO: iterate through attributes to locate ERROR-CODE...
      }
      else {
        // Neither success nor error
        puts("Neither a \'binding success\' nor a \'binding error\'... HOW???");
      }
    }
    else {
      // somehow received nothing... or worse...
      perror("num_bytes_received <= 0\n");
      i--;
    }
  }
  return NULL;
}

void build_port_spoof() {
  uint16_t stun_type = htons(0x0001);
  uint16_t stun_length = htons(0x0008);
  uint32_t stun_magic_cookie = htonl(0x2112A442);
  uint32_t stun_change_request_header = htonl(0x00030004); // type 3, size 4
  uint32_t stun_change_request_attribute = htonl(0x00000002); // type 010, port-only

  memmove(request_buffer, &stun_type, 2);
  memmove(request_buffer + 2, &stun_length, 2);
  memmove(request_buffer + 4, &stun_magic_cookie, 4);
  memmove(request_buffer + 20, &stun_change_request_header, 4);
  memmove(request_buffer + 24, &stun_change_request_attribute, 4);
}

void build_both_spoof() {
  uint16_t stun_type = htons(0x0001);
  uint16_t stun_length = htons(0x0008);
  uint32_t stun_magic_cookie = htonl(0x2112A442);
  uint32_t stun_change_request_header = htonl(0x00030004); // type 3, size 4
  uint32_t stun_change_request_attribute = htonl(0x00000006); // type 110, both

  memmove(request_buffer, &stun_type, 2);
  memmove(request_buffer + 2, &stun_length, 2);
  memmove(request_buffer + 4, &stun_magic_cookie, 4);
  memmove(request_buffer + 20, &stun_change_request_header, 4);
  memmove(request_buffer + 24, &stun_change_request_attribute, 4);
}