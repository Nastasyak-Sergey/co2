#include "../inc/systick.h"
#include "../inc/common.h"
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>

#define SYSTICK_FREQ		1e3 /* overflows per second */
#define SYSTICK_RELOAD_VAL	(rcc_ahb_frequency / SYSTICK_FREQ)
#define AHB_TICKS_PER_USEC	(rcc_ahb_frequency / 1e6)
#define USEC_PER_MSEC		1e3

static volatile uint32_t ticks;

/**
 * Systick handler.
 *
 * Since systick handler is an entry of vector table, it is also copied
 * to RAM during vector table relocation. However there is no possibility
 * to request systick by its irq number when interacting with irq manager
 * for the reason that systick is implemented inside Cortex-M3 core, and is
 * considered primarely as exception, not an interrupt. As systick handler
 * is defined as a weak symbol, caller should redefine it.
 */
void sys_tick_handler(void)
{
	ticks++;
}

/*
 * Precision: +/-1 msec.
 */
uint32_t systick_get_time_ms(void)
{
	return ticks;
}

uint32_t systick_get_time_us(void)
{
	uint32_t us;

	us = (SYSTICK_RELOAD_VAL - systick_get_value()) / AHB_TICKS_PER_USEC;
	us += ticks * USEC_PER_MSEC;

	return us;
}

/* Calculate timestamp difference in milliseconds */
uint32_t systick_calc_diff_ms(uint32_t t1, uint32_t t2)
{
	if (t1 > t2)
		t2 += UINT32_MAX;

	return t2 - t1;
}

/**
 * Initialize systick timer.
 *
 * @return 0 or -1 on error
 */
int systick_init(void)
{
	if (!systick_set_frequency(SYSTICK_FREQ, rcc_ahb_frequency))
		return -1;

	systick_clear();
	systick_interrupt_enable();
	systick_counter_enable();

	return 0;
}

/**
 * De-initialize systick timer.
 */
void systick_exit(void)
{
	systick_counter_disable();
	systick_interrupt_disable();
}
