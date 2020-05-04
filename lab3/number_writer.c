/* A program for outputting to stdout a series of numbers
 * starting from 0 and up to the input number, delimited by
 * a newline or a comma+space. Devised for testing the
 * mrt module.
 *
 * command line:
 *	number_writer max_number newline(0)/comma+space(1)
 *	
 * For Dartmouth COSC 60 Lab 3;
 * By Shengsong Gao, April 2020.
 */

#include <stdio.h>
#include <stdlib.h> // atoi()

int main(int argc, char const *argv[]) {
  /****** parsing arguments ******/
	if (argc != 3) {
		fprintf(stderr, "usage: %s max_number newline(0)/comma+space(1)\n", argv[0]);
		return -1;
	}
	int max_number = atoi(argv[1]);
  int delimiter_choice = atoi(argv[2]);
  char *delimiter;

  switch (delimiter_choice) {
    case 0 :
      delimiter = "\n";
      break;
    case 1 :
      delimiter = ", ";
      break;
    default :
      fprintf(stderr, "usage: %s max_number newline(0)/comma+space(1)\n", argv[0]);
		  return -1;
  }

  for (int i = 0; i <= max_number; i++) {
    printf("%d%s", i, delimiter);
  }

  return 0;
}
