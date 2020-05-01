/* Utility functions for the MRT module.
 * 
 * 
 * For Dartmouth COSC 60 Lab 3;
 * By Shengsong Gao, April 2020.
 */

// Reference: http://www.cse.yorku.ca/~oz/hash.html
unsigned long
hash(unsigned char *str)
{
  unsigned long hash = 5381;
  int c;

  while (c = *str++) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }

  return hash;
}
