/* Common definitions shared between sender and receiver
 * Mini Reliable Transport modules.
 *
 * For Dartmouth COSC 60 Lab 3;
 * By Shengsong Gao, April 2020.
 */

#ifndef _mrt_h
#define _mrt_h

// types of transmissions
#define MRT_UNKN 0
#define MRT_RCON 1
#define MRT_ACON 2
#define MRT_DATA 3
#define MRT_ADAT 4
#define MRT_RCLS 5
#define MRT_ACLS 6

#define MRT_HASH_LENGTH           8     // unsigned long
#define MRT_TYPE_LENGTH           4     // int
#define MRT_FRAGMENT_LENGTH       4     // int
#define MRT_WINDOWSIZE_LENGTH     4     // int
#define MRT_HEADER_LENGTH         (MRT_HASH_LENGTH + MRT_TYPE_LENGTH + MRT_FRAGMENT_LENGTH + MRT_WINDOWSIZE_LENGTH)

#define MRT_TYPE_LOCATION        MRT_HASH_LENGTH
#define MRT_FRAGMENT_LOCATION    (MRT_TYPE_LOCATION + MRT_TYPE_LENGTH)
#define MRT_WINDOWSIZE_LOCATION  (MRT_FRAGMENT_LOCATION + MRT_FRAGMENT_LENGTH)
#define MRT_PAYLOAD_LOCATION     MRT_HEADER_LENGTH

/* references for MAX_UDP_PAYLOAD_LENGTH:
 * https://stackoverflow.com/questions/14993000/the-most-reliable-and-efficient-udp-packet-size
 * https://stackoverflow.com/questions/1098897/what-is-the-largest-safe-udp-packet-size-on-the-internet
 * it's supposedly the maximum allowed such that it is NEVER
 * fragmented.
 */
#define MAX_UDP_PAYLOAD_LENGTH   508
#define MAX_MRT_PAYLOAD_LENGTH   (MAX_UDP_PAYLOAD_LENGTH - MRT_HEADER_LENGTH)

// consistently less than 0.4ms with `ping -s 64000 localhost`
// average RTT is about 100ms to Google... so...
#define EXPECTED_RTT             1000000  // MICROSECONDS... for usleep()

// variables initialized in mrt.c; for memmove() use
const int unkn_type;
const int rcon_type;
const int acon_type;
const int data_type;
const int adat_type;
const int rcls_type;
const int acls_type;

#endif // _mrt_h
