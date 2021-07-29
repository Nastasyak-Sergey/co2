#ifndef COMMON_H
#define COMMON_H

#include "systick.h"
#include <libopencm3/cm3/assert.h>

#define BIT(n)			(1 << (n))
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(a[0]))
#define UNUSED(x)		((void)x)


void inplace_reverse(char *str);

/**
 * Enter critical section (store IRQ flags and disable interrupts).
 *
 * This function serves a purpose of guarding some code from being interrupted
 * by ISR. For example it can be used to safely read/write variable which is
 * also being changed in ISR. Note that it's not necessary to mark such
 * variables with "volatile" modifier when already guarding it with critical
 * section (refer to @ref exit_critical() documentation for details). Keep the
 * critical section as small and fast as possible!
 *
 * The @p flags parameter indicates whether interrupts are enabled or disabled
 * right now (before disabling interrupts). It's often desired to restore such
 * interrupts enabled/disabled state instead of just enabling interrupts. This
 * function stores IRQ flags into provided variable instead of using global
 * variable, to avoid re-entrance issues.
 *
 * Memory barriers are not needed when disabling interrupts (see AN321).
 *
 * @param[out] flags Will contain IRQ flags value before disabling interrupts;
 *                   must have "unsigned long" type
 */
#define enter_critical(flags)						        \
do {									                    \
	__asm__ __volatile__ (						            \
		"mrs %0, primask\n"	/* save PRIMASK to "flags" */	\
		"cpsid i"		/* disable interrupts */	        \
		: "=r" (flags)						                \
		:							                        \
		: "memory");						                \
} while (0)

/**
 * Exit critical section (restore saved IRQ flags).
 *
 * Restores interrupts state (enabled/disabled) stored in @ref enter_critical().
 *
 * Contains memory barriers:
 *   - compiler barrier ("memory"): to prevent caching the values in registers;
 *     this way we don't have to use "volatile" modifier for variables that
 *     are being changed in ISRs
 *   - processor barrier (ISB): so that pending interrupts are run right after
 *     ISB instruction (as recommended by ARM AN321 guide)
 *
 * @param[in] flags Previously saved IRQ flags.
 */
#define exit_critical(flags)						        \
do {									                    \
	__asm__ __volatile__ (						            \
		"msr primask, %0\n" /* load PRIMASK from "flags" */	\
		"isb"							                    \
		:							                        \
		: "r" (flags)						                \
		: "memory");						                \
} while (0)

/**
 * Wait for some event (condition) to happen, breaking off on timeout.
 *
 * This is blocking wait, of course, as we don't use context switching.
 * Be aware of watchdog interval!
 *
 * @param cond C expression (condition) for the event to wait for
 * @param timeout Value given in msec
 * @return 0 if condition is met or -1 on timeout
 */
#define wait_event_timeout(cond, timeout)				    \
({									                        \
	uint32_t _t1;							                \
	int _ret = 0;							                \
									                        \
	_t1 = systick_get_time_ms();					        \
									                        \
	while (!(cond)) {						                \
		uint32_t _t2 = systick_get_time_ms();			    \
		uint32_t _elapsed = systick_calc_diff_ms(_t1, _t2);	\
									                        \
		if (_elapsed > (timeout)) {				            \
			_ret = -1;					                    \
			break;						                    \
		}							                        \
	}								                        \
									                        \
	_ret;								                    \
})

/** CPU cycles per 1 iteration of loop in ldelay() */
#define CYCLES_PER_LOOP		9UL // 3UL for 24MHz
/** How many CPU cycles to wait for 1 usec */
//#define CYCLES_PER_USEC		24UL	   /* for 24 MHz CPU frequency */
#define CYCLES_PER_USEC		    72UL	   /* for 72 MHz CPU frequency */
/** Delay for "d" micro-seconds */
#define udelay(d)		ldelay((d) * CYCLES_PER_USEC)
/** Delay for "d" milliseconds */
#define mdelay(d)		udelay((d) * 1000UL)

/**
 * Delay for "cycles" number of machine cycles.
 *
 * @param cycles Number of CPU cycles to delay (must be >= CYCLES_PER_LOOP)
 *
 * @note Interrupts should be disabled during this operation
 */
static inline __attribute__((always_inline)) void ldelay(unsigned long cycles)
{
	unsigned long loops = cycles / CYCLES_PER_LOOP;

	cm3_assert(cycles >= CYCLES_PER_LOOP);

	__asm__ __volatile__
			("1:\n" "subs %0, %1, #1\n"
			"bne 1b"
			: "=r" (loops)
			: "0" (loops));
}

/* The "volatile" is due to gcc bugs */
#define barrier() __asm__ __volatile__ ("":::"memory")

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"

/* Don't use it; use READ_ONCE() instead */
static inline __attribute__((always_inline))
void __read_once_size(const volatile void *p, void *res, int size)
{
	switch (size) {
	case 1: *(uint8_t *)res = *(volatile uint8_t *)p; break;
	case 2: *(uint16_t *)res = *(volatile uint16_t *)p; break;
	case 4: *(uint32_t *)res = *(volatile uint32_t *)p; break;
	case 8: *(uint64_t *)res = *(volatile uint64_t *)p; break;
	default:
		barrier();
		__builtin_memcpy((void *)res, (const void *)p, size);
		barrier();
	}
}

/* Don't use it; use WRITE_ONCE() instead */
static inline __attribute__((always_inline))
void __write_once_size(volatile void *p, void *res, int size)
{
	switch (size) {
	case 1: *(volatile uint8_t *)p = *(uint8_t *)res; break;
	case 2: *(volatile uint16_t *)p = *(uint16_t *)res; break;
	case 4: *(volatile uint32_t *)p = *(uint32_t *)res; break;
	case 8: *(volatile uint64_t *)p = *(uint64_t *)res; break;
	default:
		barrier();
		__builtin_memcpy((void *)p, (const void *)res, size);
		barrier();
	}
}

#pragma GCC diagnostic pop

/*
 * Prevent the compiler from merging or refetching reads or writes. The
 * compiler is also forbidden from reordering successive instances of
 * READ_ONCE and WRITE_ONCE, but only when the compiler is aware of some
 * particular ordering. One way to make the compiler aware of ordering is to
 * put the two invocations of READ_ONCE or WRITE_ONCE in different C
 * statements.
 *
 * These two macros will also work on aggregate data types like structs or
 * unions. If the size of the accessed data type exceeds the word size of
 * the machine (e.g., 32 bits or 64 bits) READ_ONCE() and WRITE_ONCE() will
 * fall back to memcpy(). There's at least two memcpy()s: one for the
 * __builtin_memcpy() and then one for the macro doing the copy of variable
 * - '__u' allocated on the stack.
 *
 * Their two major use cases are: (1) Mediating communication between
 * process-level code and irq/NMI handlers, all running on the same CPU,
 * and (2) Ensuring that the compiler does not fold, spindle, or otherwise
 * mutilate accesses that either do not require ordering or that interact
 * with an explicit memory barrier or atomic instruction that provides the
 * required ordering.
 */

#define READ_ONCE(x)							    \
({									                \
	union { typeof(x) __val; char __c[1]; } __u;	\
	__read_once_size(&(x), __u.__c, sizeof(x));		\
	__u.__val;							            \
})

#define WRITE_ONCE(x, val)						    \
({									                \
	union { typeof(x) __val; char __c[1]; } __v =	\
		{ .__val = (typeof(x))(val) };				\
	__write_once_size(&(x), __v.__c, sizeof(x));	\
	__v.__val;							            \
})

void __attribute__((__noreturn__)) hang(void);

#endif /* TOOLS_COMMON_H */
