/* Header file for `utilities.c`
 * Utility functions for the MRT module.
 * 
 * For Dartmouth COSC 60 Lab 3;
 * By Shengsong Gao, April 2020.
 */

#ifndef _utilities_h
#define _utilities_h

/* the djb2 hash function
 * reference: http://www.cse.yorku.ca/~oz/hash.html
 *
 * expects the str to be NULL-terminated
 */
unsigned long
hash(char *str);

#endif // _utilities_h