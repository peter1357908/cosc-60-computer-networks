/* STUN client for the STUN NAT-detector module
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

#define STUN_CLIENT_PORT        4242

#define STUN_HEADER_SIZE        20
#define STUN_BUFFER_SIZE        (STUN_HEADER_SIZE + 1000)
#define HOSTNAME_HOLDER_SIZE    100

/****** function declarations ******/
void *response_handler(void *);
void build_binding_request();
void send_request_to(const char *, uint16_t);

/****** global variables ******/
/* 4 from stun.voipstunt.com:3478
 * 5 from 5 google STUN servers (RegEx: stun?.l.google.com:19302)
 */
int num_response_expected = 9;
unsigned int addr_len = (unsigned int) sizeof(struct sockaddr_in);
int client_sockfd;
char request_buffer[STUN_BUFFER_SIZE] = {0};

int should_close = 0;
pthread_mutex_t should_close_lock;

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

  // flood 'em with requests...
  build_binding_request();
  send_request_to("stun.voipstunt.com", 3478);
  send_request_to("stun.l.google.com", 19302);
  send_request_to("stun1.l.google.com", 19302);
  send_request_to("stun2.l.google.com", 19302);
  send_request_to("stun3.l.google.com", 19302);
  send_request_to("stun4.l.google.com", 19302);

  pthread_join(response_handler_thread, NULL);

  return 0;
}

// assumes that the received messages are all STUN messages
void *response_handler(void *_nil) {
  char response_buffer[STUN_BUFFER_SIZE] = {0}; // in network byte order
  struct sockaddr_in addr_holder = {0};
  unsigned int addr_len_holder = addr_len;
  char hostname_holder[HOSTNAME_HOLDER_SIZE] = {0}; // holds IPv4 addresses
  struct sockaddr_in nat_addr_holder = {0};
  nat_addr_holder.sin_family = AF_INET; // getnameinfo() needs this filled

  for (int i = 0; i < num_response_expected; i++) {
    int num_bytes_received = recvfrom(client_sockfd, response_buffer,
                      STUN_BUFFER_SIZE, 0, (struct sockaddr *)(&addr_holder),
                      &addr_len_holder);

    getnameinfo((struct sockaddr *)(&addr_holder), addr_len, hostname_holder, HOSTNAME_HOLDER_SIZE, NULL, 0, NI_NUMERICHOST);
    printf("STUN server %s responded:\n", hostname_holder);
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
          // TODO: why at the third attribute, the length becomes 0????????
          uint16_t attribute_length = ntohs(((uint16_t *)response_attribute_location)[1]);
          // printf("attribute_length=%hu\n", attribute_length);
          // check if it is a MAPPED-ADDRESS
          if (((uint16_t *)response_attribute_location)[0] == htons(0x0001)) {
            nat_addr_holder.sin_port = ((uint16_t *)response_attribute_location)[3];
            nat_addr_holder.sin_addr.s_addr = ((uint32_t *)response_attribute_location)[2];
            getnameinfo((struct sockaddr *)(&nat_addr_holder), addr_len, hostname_holder, HOSTNAME_HOLDER_SIZE, NULL, 0, NI_NUMERICHOST);
            printf("FOUND IPv4 MAPPED-ADDRESS:\nNAT IP: %s, NAT PORT: %hu\n", hostname_holder, ntohs(nat_addr_holder.sin_port));
          }
          // check if it is an XOR-MAPPED-ADDRESS
          else if (((uint16_t *)response_attribute_location)[0] == htons(0x0020)) {
            uint32_t stun_magic_cookie = htonl(0x2112A442);
            uint16_t xor_port = ((uint16_t *)response_attribute_location)[3];
            nat_addr_holder.sin_port = (xor_port ^ ((uint16_t) stun_magic_cookie));
            uint32_t xor_addr = ((uint32_t *)response_attribute_location)[2];
            // TODO: EXTREMELY WEIRD SHIFTING LOGIC... NEED FURTHER INSPECTION...
            nat_addr_holder.sin_addr.s_addr = (xor_addr ^ stun_magic_cookie);
            getnameinfo((struct sockaddr *)(&nat_addr_holder), addr_len, hostname_holder, HOSTNAME_HOLDER_SIZE, NULL, 0, NI_NUMERICHOST);
            printf("FOUND IPv4 XOR-MAPPED-ADDRESS:\nNAT IP: %s, NAT PORT: %hu\n", hostname_holder, ntohs(nat_addr_holder.sin_port));
          }
          else {
            // puts("FOUND irrelevant attribute...");
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

void build_binding_request() {
  uint16_t stun_type = htons(0x0001);
  uint16_t stun_length = htons(0x0000);
  uint32_t stun_magic_cookie = htonl(0x2112A442);
  // TODO: build randomized transaction ID?
  memmove(request_buffer, &stun_type, 2);
  memmove(request_buffer + 2, &stun_length, 2);
  memmove(request_buffer + 4, &stun_magic_cookie, 4);
}

void send_request_to(const char *text_addr, uint16_t port_number) {
  int result;
  char hostname_holder[HOSTNAME_HOLDER_SIZE];
  struct sockaddr_in *stun_server_addr;
  struct addrinfo hints = {0}, *info_p, *curr_info_p;
  hints.ai_family = AF_INET; // IPv4 Only
  hints.ai_socktype = SOCK_DGRAM; // UDP Only

  if ((result = getaddrinfo(text_addr, NULL, &hints, &info_p)) != 0) {
    puts(gai_strerror(result));
    return;
  }

  for (curr_info_p = info_p; curr_info_p != NULL; curr_info_p = curr_info_p->ai_next) {
    stun_server_addr = (struct sockaddr_in *)(curr_info_p->ai_addr);
    stun_server_addr->sin_port = htons(port_number);

    sendto(client_sockfd, request_buffer, STUN_HEADER_SIZE,
            0, (const struct sockaddr *)(stun_server_addr), 
            addr_len);
    
    getnameinfo(curr_info_p->ai_addr, addr_len, hostname_holder, 
            HOSTNAME_HOLDER_SIZE, NULL, 0, NI_NUMERICHOST);
    // printf("just sent a STUN binding request to %s:%hu\n", hostname_holder, ntohs(stun_server_addr->sin_port));
  }
  freeaddrinfo(info_p);
}


// otherwise we could do something like this...
//
// stun_server_addr.sin_family = AF_INET; 
// stun_server_addr.sin_port = htons(19302);
// if (inet_pton(AF_INET, "74.125.204.127", &(stun_server_addr.sin_addr)) != 1) {
//   perror("inet_pton() failed\n");
//   return -1;
// }
//
// now get it back and print it
// inet_ntop(AF_INET, &(stun_server_addr.sin_addr), str, INET_ADDRSTRLEN);
