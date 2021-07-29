
#include "common.h"
#include "libprintf/printf.h"
#include <string.h>

/**
 *  Reverse the given null-terminated string in place.
 *
 *  Swap the values in the two given variables.
 *  Fails when a and b refer to same memory location.
 *  This works because of three basic properties of XOR:
 *  x ^ 0 = x, x ^ x = 0 and x ^ y = y ^ x for all values x and y
 *
 *  @param input should be an array, whose contents are initialized to
 *  the given string constant.
 */
void inplace_reverse(char *str)
{
	if (str) {
		char *end = str + strlen(str) - 1;

#define XOR_SWAP(a,b) do	\
		{		\
			a ^= b;	\
			b ^= a;	\
			a ^= b;	\
		} while (0)

		/* Walk inwards from both ends of the string,
		 * swapping until we get to the middle
		 */
		while (str < end) {
			XOR_SWAP(*str, *end);
			str++;
			end--;
		}
#undef XOR_SWAP
	}
}

/**
 * Error handling routine.
 *
 * Prints error message and hang in endless loop.
 */
void __attribute__((__noreturn__)) hang(void)
{
    printf("Error: Reboot your board");
	for (;;)
		;
}
