/* Library with functions for the Dual-Purpose Protocol.
 *
 * For Dartmouth COSC 60 Lab 2;
 * By Shengsong Gao, 2020.
 */

#include <stdio.h>
#include <string.h>
#include "dpp.h"

#define DPP_MESSAGE "MSG"
#define DPP_JOIN "HELO"
#define DPP_QUIT "BYE"

int build_message(char *buffer, const char *username, char *message) {
	return sprintf(buffer, "%s%s%s%s%s", DPP_MESSAGE,
			DPP_DELIMITER, username, DPP_DELIMITER, message);
}

int build_join(char *buffer, const char *username) {
	return sprintf(buffer, "%s%s%s", DPP_JOIN, DPP_DELIMITER, username);
}

int build_quit(char *buffer, const char *username) {
	return sprintf(buffer, "%s%s%s", DPP_QUIT, DPP_DELIMITER, username);
}

int parse_transmission(char *transmission_buffer, char **username_pointer, char **message_pointer, int num_char_received) {
	char *delimiter_start_location;
	char *type = transmission_buffer;
	int delimiter_length = strlen(DPP_DELIMITER);
	
	/** hard-coded parsing **/
	// first, ensure that the transmission is null-terminated
	transmission_buffer[num_char_received] = '\0';
	
	// split the "type" off and locate the "username"
	delimiter_start_location = strstr(type, DPP_DELIMITER);
	if (delimiter_start_location == NULL) { return DPP_UNKNOWN_TYPE; }
	*delimiter_start_location = '\0';
	*username_pointer = delimiter_start_location + delimiter_length;
	
	// split the "username" off and locate the "message"
	delimiter_start_location = strstr(*username_pointer, DPP_DELIMITER);
	if (delimiter_start_location != NULL) {
		if (strcmp(type, DPP_MESSAGE) == 0) {
			*delimiter_start_location = '\0';
			*message_pointer = delimiter_start_location + delimiter_length;

			return DPP_MESSAGE_TYPE;
		} else {
			// the transmission is not a MSG yet has a second delimiter...
			return DPP_UNKNOWN_TYPE;
		}
	} else if (strcmp(type, DPP_JOIN) == 0) {
		return DPP_JOIN_TYPE;
	} else if (strcmp(type, DPP_QUIT) == 0) {
		return DPP_QUIT_TYPE;
	} else {
		return DPP_UNKNOWN_TYPE;
	}
}

int is_quit(char *transmission_buffer) {
	if (strncmp(transmission_buffer, DPP_QUIT, strlen(DPP_QUIT)) == 0) {
		return 1;
	}
	return 0;
}
 
 
 