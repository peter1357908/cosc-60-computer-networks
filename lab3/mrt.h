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

#define MRT_ERROR     -78
#define MRT_SUCCESS   -42

#define MRT_HASH_LENGTH           8     // unsigned long
#define MRT_TYPE_LENGTH           4     // int
#define MRT_FRAGMENT_LENGTH       4     // int
#define MRT_ID_LENGTH             4     // int
#define MRT_WINDOWSIZE_LENGTH     4     // int
#define MRT_HEADER_SIZE           (MRT_HASH_LENGTH + MRT_TYPE_LENGTH + MRT_FRAGMENT_LENGTH + MRT_ID_LENGTH + MRT_WINDOWSIZE_LENGTH)

#define MRT_TYPE_LOCATION        MRT_HASH_LENGTH
#define MRT_FRAGMENT_LOCATION    (MRT_TYPE_LOCATION + MRT_TYPE_LENGTH)
#define MRT_ID_LOCATION          (MRT_FRAGMENT_LOCATION + MRT_FRAGMENT_LENGTH)
#define MRT_WINDOWSIZE_LOCATION  (MRT_ID_LOCATION + MRT_ID_LENGTH)
#define MRT_PAYLOAD_LOCATION     MRT_HEADER_SIZE

#define MAX_UDP_PAYLOAD_SIZE            65507 // IPv4: 65507, IPv6: 65527
#define MAX_MRT_PAYLOAD_SIZE            (MAX_UDP_PAYLOAD_SIZE - MRT_SIZE)


// variables initialized in mrt.c; for memmove() use
const int unkn_type;
const int rcon_type;
const int acon_type;
const int data_type
const int adat_type;
const int rcls_type;
const int acls_type;


#endif // _mrt_h
