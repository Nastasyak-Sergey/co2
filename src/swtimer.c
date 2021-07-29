/**
 * @file
 *
 * Software timers framework.
 *
 * Can be used by many users, but uses only one single hardware (general
 * purpose) timer underneath. Users can register software timer, and this
 * framework will call registered function when registered timeout expires.
 */

#include "swtimer.h"
#include "irq.h"
#include "sched.h"
#include "common.h"
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/timer.h>
#include <stddef.h>
#include <string.h>


#include "libprintf/printf.h"


#define SWTIMER_TIMERS_MAX	10
#define SWTIMER_TASK		"swtimer"

/* Software timer parameters */
struct swtimer_sw_tim {
	swtimer_callback_t cb;	/* function to call when this timer overflows */
	void *data;		        /* user private data passed to cb */
	int period;		        /* timer overflow period, msec */
	int remaining;		    /* remaining time till overflow, msec */
	bool active;		    /* if true, callback will be executed */
};

/* Driver struct (swtimer framework) */
struct swtimer {
	struct swtimer_hw_tim hw_tim;
	struct irq_action action;
	struct swtimer_sw_tim timer_list[SWTIMER_TIMERS_MAX];
	volatile int ticks;		/* global ticks counter */
	int task_id;			/* scheduler task ID */
};

/* Singleton driver object */
static struct swtimer swtimer;

/* -------------------------------------------------------------------------- */

static irqreturn_t swtimer_isr(int irq, void *data)
{
	struct swtimer *obj = (struct swtimer *)(data);

	UNUSED(irq);

	/*
	 * For some reason, sometimes we catch the interrupts with CC1IF..CC4IF
	 * flags set in TIM2_SR register (though we didn't enable such
	 * interrupts in TIM2_DIER). It can be some errata, but anyway, let's
	 * increment ticks only on "Update" interrupt flag.
	 */
	if (!timer_get_flag(obj->hw_tim.base, TIM_SR_UIF))
		return IRQ_NONE;

	obj->ticks = SWTIMER_HW_OVERFLOW;
	sched_set_ready(obj->task_id);
	timer_clear_flag(obj->hw_tim.base, TIM_SR_UIF);

	return IRQ_HANDLED;
}

static void swtimer_task(void *data)
{
	size_t i;
	struct swtimer *obj = (struct swtimer *)data;

	for (i = 0; i < SWTIMER_TIMERS_MAX; i++) {
		if (!obj->timer_list[i].active)
			continue;
		if (obj->timer_list[i].remaining <= 0) {
			obj->timer_list[i].cb(obj->timer_list[i].data);
			obj->timer_list[i].remaining = obj->timer_list[i].period;
		}
		obj->timer_list[i].remaining -= obj->ticks;
	}
	obj->ticks = 0;
}

static int swtimer_find_empty_slot(struct swtimer *obj)
{
	size_t i;

	for (i = 0; i < SWTIMER_TIMERS_MAX; ++i) {
		if (!obj->timer_list[i].cb)
			return i;
	}

	return -1;
}

static void swtimer_hw_init(struct swtimer *obj)
{
	rcc_periph_reset_pulse(obj->hw_tim.rst);

	timer_set_mode(obj->hw_tim.base, TIM_CR1_CKD_CK_INT,    // Clock division - CK_INT
		       TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);           // CMS: Center-aligned mode selection - Edge-aligned mode //DIR: Direction - Counter used as upcounter
	timer_set_prescaler(obj->hw_tim.base, obj->hw_tim.psc); // 
	timer_set_period(obj->hw_tim.base, obj->hw_tim.arr);
	timer_disable_preload(obj->hw_tim.base);
	timer_continuous_mode(obj->hw_tim.base);
	timer_enable_update_event(obj->hw_tim.base);
	timer_update_on_overflow(obj->hw_tim.base);
	timer_enable_irq(obj->hw_tim.base, TIM_DIER_UIE); // TIM DMA/Interrupt enable register, Update interrupt enable 

	nvic_set_priority(obj->hw_tim.irq, 1);
	nvic_enable_irq(obj->hw_tim.irq);

	timer_enable_counter(obj->hw_tim.base);
}

/* -------------------------------------------------------------------------- */

/**
 * Reset internal tick counter.
 *
 * This function can be useful e.g. in case when swtimer_init() was called
 * early, and when system initialization is complete, the timer tick counter is
 * non-zero value, but it's unwanted to call sw timer callbacks yet.
 */
void swtimer_reset(void)
{
	swtimer.ticks = 0; /* Set global ticks counter to 0 */
}

/**
 * Register software timer and start it immediately.
 *
 * @param cb Timer callback; will be executed when timer is expired
 * @param period Timer period, msec; minimal period (and granularity): 1 msec
 * @return Timer ID (handle) starting from 1, or negative value on error
 *
 * @note This function can be used before swtimer_init()
 */
int swtimer_tim_register(swtimer_callback_t cb, void *data, int period)
{
	int slot;

	cm3_assert(cb != NULL);
	cm3_assert(period >= SWTIMER_HW_OVERFLOW);

	slot = swtimer_find_empty_slot(&swtimer);
	if (slot < 0)
		return -1;

	swtimer.timer_list[slot].cb = cb;
	swtimer.timer_list[slot].data = data;
	swtimer.timer_list[slot].period = period;
	swtimer.timer_list[slot].remaining = period;
	swtimer.timer_list[slot].active = true;

	return slot + 1;
}

/**
 * Delete specified timer.
 *
 * @param id Timer handle
 */
void swtimer_tim_del(int id)
{
	int slot = id - 1;

	cm3_assert(slot >= 0 && slot < SWTIMER_TIMERS_MAX);
	memset(&swtimer.timer_list[slot], 0, sizeof(struct swtimer_sw_tim));
}

/**
 * Reset and start specified timer.
 *
 * @param id Timer handle
 */
void swtimer_tim_start(int id)
{
	int slot = id - 1;

	cm3_assert(slot >= 0 && slot < SWTIMER_TIMERS_MAX);
	swtimer.timer_list[slot].active = true;
}

/**
 * Stop specified timer.
 *
 * @param id Timer handle
 */
void swtimer_tim_stop(int id)
{
	int slot = id - 1;

	cm3_assert(slot >= 0 && slot < SWTIMER_TIMERS_MAX);
	swtimer.timer_list[slot].active = false;
}

/**
 * Reset software timer by ID.
 *
 * @param id Timer handle
 */
void swtimer_tim_reset(int id)
{
	int slot = id - 1;

	cm3_assert(slot >= 0 && slot < SWTIMER_TIMERS_MAX);
	swtimer.timer_list[slot].remaining = swtimer.timer_list[slot].period;
}

/**
 * Set new pediod for timer by ID.
 *
 * @param id Timer handle
 * @param period Timer period, msec
 */
void swtimer_tim_set_period(int id, int period)
{
	int slot = id - 1;

	cm3_assert(slot >= 0 && slot < SWTIMER_TIMERS_MAX);
	swtimer.timer_list[slot].period = period;
}

/**
 * Get remaining time till the timer expires.
 *
 * @param id Timer handle
 * @return Remaining time, msec
 */
int swtimer_tim_get_remaining(int id)
{
	int slot = id - 1;

	cm3_assert(slot >= 0 && slot < SWTIMER_TIMERS_MAX);
	return swtimer.timer_list[slot].remaining;
}

/**
 * Initialize software timer framework.
 *
 * Setup underneath hardware timer and run it.
 *
 * @param[in] hw_tim Parameters of HW timer to use
 * @return 0 on success or negative number on error
 *
 * @note Scheduler must be initialized before calling this
 */
int swtimer_init(const struct swtimer_hw_tim *hw_tim)
{
	int ret;
	struct swtimer *obj = &swtimer;

	obj->hw_tim		= *hw_tim;
	obj->action.handler	= swtimer_isr;
	obj->action.irq		= hw_tim->irq;
	obj->action.name	= SWTIMER_TASK;
	obj->action.data	= obj;

	ret = irq_request(&obj->action);
	if (ret < 0){
        printf("Unable request SW Timer IRQ\n");
		return -1;
    }

    swtimer_hw_init(obj);
	
    ret = sched_add_task(SWTIMER_TASK, swtimer_task, obj, &obj->task_id);
	if (ret < 0) {
        
        printf("Unable add task\n");
		return -2;
    }
	return 0;
}

/**
 * De-initialize software timer framework.
 */
void swtimer_exit(void)
{
	timer_disable_counter(swtimer.hw_tim.base);
	timer_disable_irq(swtimer.hw_tim.base, TIM_DIER_UIE);
	nvic_disable_irq(swtimer.hw_tim.irq);
	sched_del_task(swtimer.task_id);
	irq_free(&swtimer.action);
	UNUSED(swtimer);
}
