/* Header file for "dpp.c"
 * 
 * Functions for the Dual-Purpose Protocol.
 *
 * For Dartmouth COSC 60 Lab 2;
 * By Shengsong Gao, 2020.
 */

#define DPP_DELIMITER "::"
#define DPP_MAX_OVERHEAD 8 // depends on the longest flag and the delimiter

// return values for parse_transmission()
#define DPP_UNKNOWN_TYPE	-1
#define DPP_MESSAGE_TYPE	0
#define DPP_JOIN_TYPE		1
#define DPP_QUIT_TYPE		2

/* Each of the following "build" functions:
 *
 * does not check input sanity (assumes that the buffer
 * is long enough, username is legal, etc.)
 *
 * returns the number of characters written if successful 
 * (excluding the terminating '\n'); returns a negative
 * number in case of failure.
 *
 * NOTE: "message" shall not be the same as "buffer"!
 */
int build_message(char *buffer, const char *username, char *message);

int build_join(char *buffer, const char *username);

int build_quit(char *buffer, const char *username);


/* "username_pointer" and "message_pointer" point to
 * an address to the respective parts in "transmission_buffer"
 *
 * "transmission_buffer" will be altered so the respective
 * parts are null-terminated (and can be treated as separate strings)
 *
 * returns the type of the transmission (defined above with "#define")
 */
int parse_transmission(char *transmission_buffer, char **username_pointer, char **message_pointer, int num_char_received);
 
 