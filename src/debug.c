#include <string.h>
#include <stdbool.h>
#include "../inc/debug.h"

#define DEBUG_BUFFER_SIZE 80

static char debugBuffer[DEBUG_BUFFER_SIZE + 1];	//  Buffer to hold output before flushing.
static bool logEnabled = false;  //  Logging is off by default

void enable_log(void) { logEnabled = true; debugBuffer[0] = 0; }
void disable_log(void) { logEnabled = false; debugBuffer[0] = 0; }


//  ARM Semihosting code from 
//  http://www.keil.com/support/man/docs/ARMCC/armcc_pge1358787046598.htm
//  http://www.keil.com/support/man/docs/ARMCC/armcc_pge1358787048379.htm
//  http://www.keil.com/support/man/docs/ARMCC/armcc_chr1359125001592.htm
//  https://wiki.dlang.org/Minimal_semihosted_ARM_Cortex-M_%22Hello_World%22

static int __semihost(int command, void* message) {
	//  Send an ARM Semihosting command to the debugger, e.g. to print a message.
	//  To see the message you need to run opencd:
	//    openocd -f interface/stlink-v2.cfg -f target/stm32f1x.cfg -f scripts/connect.ocd

	//  Warning: This code will trigger a breakpoint and hang unless a debugger is connected.
	//  That's how ARM Semihosting sends a command to the debugger to print a message.
	//  This code MUST be disabled on production devices.
    if (!logEnabled) return -1;
    __asm( 
      "mov r0, %[cmd] \n"
      "mov r1, %[msg] \n" 
      "bkpt #0xAB \n"
	:  //  Output operand list: (nothing)
	:  //  Input operand list:
		[cmd] "r" (command), 
		[msg] "r" (message)
	:  //  Clobbered register list:
		"r0", "r1", "memory"
	);
	return 0;
}

//  ARM Semihosting code from https://github.com/ARMmbed/mbed-os/blob/master/platform/mbed_semihost_api.c

// ARM Semihosting Commands
// #define SYS_OPEN   (0x1)
// #define SYS_CLOSE  (0x2)
#define SYS_WRITE  (0x5)
// #define SYS_READ   (0x6)
// #define SYS_ISTTY  (0x9)
// #define SYS_SEEK   (0xa)
// #define SYS_ENSURE (0xb)
// #define SYS_FLEN   (0xc)
// #define SYS_REMOVE (0xe)
// #define SYS_RENAME (0xf)
// #define SYS_EXIT   (0x18)

// We normally set the file handle to 2 to write to the debugger's stderr output.
#define SEMIHOST_HANDLE 2

static int semihost_write(uint32_t fh, const unsigned char *buffer, unsigned int length) {
    //  Write "length" number of bytes from "buffer" to the debugger's file handle fh.
    //  We normally set fh=2 to write to the debugger's stderr output.
    if (length == 0) { return 0; }
    uint32_t args[3];
    args[0] = (uint32_t)fh;
    args[1] = (uint32_t)buffer;
    args[2] = (uint32_t)length;
    return __semihost(SYS_WRITE, args);
}

static void debug_append(const char *buffer, unsigned int length) {
    //  Append "length" number of bytes from "buffer" to the debug buffer.
    const int debugBufferLength = strlen(debugBuffer);
    //  If can't fit into buffer, just send to the debugger log now.
    if (debugBufferLength + length >= DEBUG_BUFFER_SIZE) {
        debug_flush();
        semihost_write(SEMIHOST_HANDLE, (const unsigned char *) buffer, length);
        return;
    }
    //  Else append to the buffer.
    strncat(debugBuffer, buffer, length);
    debugBuffer[debugBufferLength + length] = 0;  //  Terminate the string.
}

void debug_flush(void) {
    //  Flush the debug buffer to the debugger log.  This will be slow.
    if (debugBuffer[0] == 0) { return; }  //  Debug buffer is empty, nothing to write.
	semihost_write(SEMIHOST_HANDLE, (const unsigned char *) debugBuffer, strlen(debugBuffer));
    debugBuffer[0] = 0;
}

void _putchar(char character)
{
    debug_append((const char *) &character, 1);
    if (character == '\n') {
       debug_flush();
    }
}

