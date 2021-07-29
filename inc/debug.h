//  Log messages to the debug console.  Note: Logging will be buffered in memory.  Messages will not 
//  be displayed until debug_flush() is called.
#ifndef DEBUG_H_
#define DEBUG_H_

#include <stdint.h>  //  For uint8_t
#include <stdlib.h>  //  For size_t

#if !defined(DEBUG) || !DEBUG
	#define logmsg(msg, ...)
#else

	#include "libprintf/printf.h"
	enable_log();       //  Uncomment to allow display of debug messages
						//  in development devices
	#define logmsg(fmt, ...) do {
		printf(fmt, ##__VA_ARGS__);
	} while(0)

#endif





#ifdef __cplusplus
extern "C" {  //  Allows functions below to be called by C and C++ code.
#endif

void enable_log(void);   //  Enable the debug log.
void disable_log(void);  //  Disable the debug log.
void debug_begin(uint16_t bps);     //  Open the debug console at the specified bits per second.

void debug_write(const char * ch);    //  Write a char to the buffered debug log.
void debug_println(const char *s);  //  Write a string plus newline to the buffered debug log.
void debug_print_hex(uint8_t ch);    //  Write a char in hexadecimal to the buffered debug log.
void debug_print_int(int i);
void debug_print(size_t l);
void debug_print_char(char ch);
void debug_print_float(float f);    //  Note: Always prints with 2 decimal places.
void debug_flush(void);             //  Flush the buffer of the debug log so that buffered data will appear.

//extern void _putchar(char character);

#ifdef __cplusplus
}
#endif  //  __cplusplus

#endif  //  DEBUG_H_
