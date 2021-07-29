/**
 * @file
 *
 * Interrupt management subsystem.
 *
 * Provides a way to request an interrupt by its number for multiple users,
 * and also pass the registered user data to ISR when it's called.
 *
 * This file is architecture dependent (has Cortex-M3 specific features and
 * definitions). See "Cortex-M3 Programming Manual" for details (chapter 2.3.4
 * Vector table).
 *
 * The design is inspired by Linux kernel interrupt subsystem.
 */

#include "irq.h"
#include "board.h"
#include "common.h"
#include <libopencm3/stm32/flash.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/vector.h>
//#include <stdio.h>
#include "libprintf/printf.h"
#include <string.h>

#define SRAM_BASE		0x20000000	/* start address of RAM */
#define V7M_xPSR_EXCEPTIONNO	0x1ff		/* ISR_NUMBER bits in IPSR */

#define irq_to_desc(irq)	(irq_desc + (irq))
#define for_each_action_of_desc(desc, act)			\
	for (act = desc->action; act; act = act->next)

struct irq_desc;
typedef void (*irq_flow_handler_t)(unsigned int irq, struct irq_desc *desc);

struct irq_desc {
	irq_flow_handler_t handle_irq;	/* high-level IRQ events handler */
	struct irq_action *action;	/* list of actions (shared pointer) */
};

/* High-level IRQ handler for spurious and unhandled IRQs */
static void irq_handle_bad(unsigned int irq, struct irq_desc *desc)
{
	UNUSED(desc);
	printf("unexpected IRQ trap at vector %d\n", irq);
}

struct irq_desc irq_desc[NVIC_IRQ_COUNT] = {
	[0 ... NVIC_IRQ_COUNT-1] = {
		.handle_irq = irq_handle_bad,
	}
};

/* High-level IRQ handler for requested IRQs */
static void irq_handle(unsigned int irq, struct irq_desc *desc)
{
	struct irq_action *action;
	irqreturn_t retval = IRQ_NONE;

	for_each_action_of_desc(desc, action) {
		irqreturn_t res;

		res = action->handler(irq, action->data);
		retval |= res;
	}

	if (retval == IRQ_NONE)
		printf("IRQ %d: nobody cared\n", irq);
}

/* Low-level IRQ handler; it's set to all IRQs in vector table */
static void __irq_entry(void)
{
	unsigned int irq;
	struct irq_desc *desc;

	/* Keep interrupts disabled during ISR */
	__asm__ __volatile__ ("cpsid i" : : : "memory");

	/* Get IRQ number */
	__asm__ __volatile__ ("mrs %0, ipsr" : "=r" (irq) : : "memory");
	irq &= V7M_xPSR_EXCEPTIONNO;
	irq -= 16;

	/* Get IRQ descriptor by IRQ number */
	desc = irq_to_desc(irq);

	/* Run high-level IRQ handler */
	desc->handle_irq(irq, desc);

	/* Enable interrupts once ISR is handled */
	__asm__ __volatile__ ("cpsie i" : : : "memory");
}

/**
 * Initialize the IRQ subsystem.
 *
 * 1. Relocate vector table from flash to RAM. It's written to the flash base by
 *    linker script, and mapped to 0x0 in runtime. It needs to be relocated to
 *    RAM, so that it's possible to modify it when user requests some interrupt
 *    and specified ISR function needs to be set in vector table.
 * 2. Replace all IRQ handlers in vector table with internal low-level handler,
 *    which in turn runs handlers registered with @ref irq_request().
 *
 * @return 0 on success or negative value on error
 */
int irq_init(void)
{
	unsigned long flags;
	vector_table_t *vtable;
	size_t i;

	enter_critical(flags);

	/* Copy vector table from flash to RAM */
	memcpy((void *)SRAM_BASE, (const void *)FLASH_BASE, CONFIG_VTOR_SIZE);

	/* Set low-level IRQ handler for all IRQs  */
	vtable = (vector_table_t *)SRAM_BASE;
	for (i = 0; i < NVIC_IRQ_COUNT; ++i)
		vtable->irq[i] = __irq_entry;

	/* Make CPU use vector table from RAM */
	SCB_VTOR = SRAM_BASE;

	exit_critical(flags);

	return 0;
}

/**
 * De-initialize IRQ subsystem.
 *
 * Once this function is called, default vector table (from flash) will be used.
 */
void irq_exit(void)
{
	size_t i;

	/* Use default vector table from flash (mapped to 0x0) */
	SCB_VTOR = 0x0;

	/* Cleanup vector table copy in RAM */
	memset((void *)SRAM_BASE, 0, CONFIG_VTOR_SIZE);

	/* Cleanup irq descriptor table */
	for (i = 0; i < NVIC_IRQ_COUNT; ++i) {
		irq_desc[i].handle_irq = irq_handle_bad;
		irq_desc[i].action = NULL;
	}
}

/**
 * Register interrupt handler for specific IRQ number.
 *
 * @param action Interrupt data; .handler, .irq and .name fields must be set;
 *		 it should be a pointer to some global variable (not stack one)
 * @return 0 on success or negative value on error
 */
int irq_request(struct irq_action *action)
{
	struct irq_desc *desc;
	unsigned long flags;

	cm3_assert(action != NULL);

	if (action->handler == NULL)
		return -1;
	if (action->irq >= NVIC_IRQ_COUNT)
		return -2;
	if (action->name == NULL)
		return -3;

	action->next = NULL;

	/*
	 * Add action in the end of the `desc->action' list and set high-level
	 * IRQ handler.
	 */
	enter_critical(flags);
	desc = irq_to_desc(action->irq);
	if (desc->action) {
		struct irq_action *a;

		a = desc->action;
		while (a->next)
			a = a->next;
		a->next = action;
	} else {
		desc->action = action;
	}
	desc->handle_irq = irq_handle;
	exit_critical(flags);

	return 0;
}

/**
 * Unregister interrupt handler.
 *
 * Remove interrupt handler that was registered before with @ref irq_request().
 *
 * @param action Pointer to irq_action that was passed in @ref irq_request()
 * @return 0 on success or negative value on error
 */
int irq_free(struct irq_action *action)
{
	struct irq_desc *desc;
	unsigned long flags;

	cm3_assert(action != NULL);

	/*
	 * Find and remove specified action from `desc->action' list and relink
	 * the list if needed.
	 */
	enter_critical(flags);
	desc = irq_to_desc(action->irq);
	if (desc->action == action) {
		desc->action = NULL;
	} else {
		struct irq_action *a;

		for_each_action_of_desc(desc, a) {
			if (a->next == action) {
				a->next = a->next->next; /* relink */
				action->next = NULL;
				exit_critical(flags);
				return 0;
			}
		}
	}
	exit_critical(flags);

	/* Specified action wasn't found in `desc->action' list */
	return -1;
}
