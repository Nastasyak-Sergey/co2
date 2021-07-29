//*
// RTC routines

#include "rtc.h"

#include <string.h>
#include <stdbool.h>
#include <libopencm3/cm3/vector.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/rtc.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/f1/bkp.h>


#include "irq.h"
#include "common.h"
#include "board.h"
#include "backup.h"
#include "libprintf/printf.h"

#define RTC_IRQS 1 // RTC_ALR, RTC_SEC 



/* Exported constants --------------------------------------------------------*/
#define CONFIGURATION_DONE 0xAAAA
#define CONFIGURATION_RESET 0x0000

/*
 *	BKP_DR4 - 0xffff0000 address available to write in flash
 *	BKP_DR5 - 0x0000ffff address available to write in flash
 *	BKP_DR6 - 0xffff0000 CRC high part
 * 	BKP_DR7 - 0x0000ffff CRC low part
 */


/*********************************************************************
 * RTC Interrupt Service Routine
 *********************************************************************/ 
static irqreturn_t rtc_isr_handler(int irq, void *data)
{
	struct rtc *obj = (struct rtc *)(data);

	UNUSED(irq);


	if ( rtc_check_flag(RTC_OW) ) {
		// Timer overflowed:
		rtc_clear_flag(RTC_OW);
	}

	if ( rtc_check_flag(RTC_SEC) ) {
		rtc_clear_flag(RTC_SEC);
		// Increment time:
//        printf("Timer Val: = %d \n", rtc_get_counter_val());
	}

	return IRQ_HANDLED;
}


/*********************************************************************
 * RTC Alarm ISR
 *********************************************************************/
static irqreturn_t rtc_alarm_isr_handler(int irq, void *data)
{

	UNUSED(irq);

    struct rtc_device *obj = (struct rtc_device *)(data);

	exti_reset_request(EXTI17);
    
    printf("ALARM !!! \n");
    
    if ( rtc_check_flag(RTC_ALR) ) {
	    rtc_clear_flag(RTC_ALR);
        printf("ALARM at %d\n", rtc_get_counter_val());
//        set_alarm(obj->alarm);
    }

    obj->cb();
    
    return IRQ_HANDLED;
}

/* Store irq objects */
static struct irq_action rtc_irq_act[RTC_IRQS] = {
	{
		.handler = rtc_alarm_isr_handler,
		.irq = NVIC_RTC_ALARM_IRQ,
		.name = "rtc_alarm_isr",
	},
/*	{
		.handler = rtc_isr_handler,
		.irq = NVIC_RTC_IRQ,
		.name = "rtc_isr,",
	}
*/
};


 /* Initialize RTC for Interrupt processing
 *********************************************************************/

int rtc_init(struct rtc_device *obj) 
{
    int ret;
	unsigned long flags;
    uint32_t reg32;

    //    32768 = 7FFFh
    //    rtc_auto_awake (RCC_LSE, 32768);	/* rtc_auto_awake (RCC_LSE, 32767); does next tasks */ 
 	
    reg32 = rcc_rtc_clock_enabled_flag();
	if (reg32 != 0) {
		rtc_awake_from_standby();
	} else {
		rtc_awake_from_off(obj->clock_source);
		rtc_set_prescale_val(obj->prescale_val);
	}
	
    /* Register interrupt handlers */
	int i;
    for (i = 0; i < RTC_IRQS; i++) {
		rtc_irq_act[i].data = (void *)obj;
		ret = irq_request(&rtc_irq_act[i]);
		if (ret < 0) {
            printf("Unable request RTC IRQ\n");
			return ret;
        }
	}

	//printf("BKP reg [0]: %x \n", BKP_DR1);
	
	if( BKP_DR1 == CONFIGURATION_RESET) {	
		
        BKP_DR1 = CONFIGURATION_DONE;
		printf("Switchen ON first time\n");
	}

	rtc_interrupt_disable(RTC_SEC);
	rtc_interrupt_disable(RTC_ALR);
	//rtc_interrupt_disable(RTC_OW);
 
    nvic_enable_irq(NVIC_RTC_ALARM_IRQ);
    nvic_set_priority(NVIC_RTC_ALARM_IRQ, 2);

	exti_set_trigger(EXTI17, EXTI_TRIGGER_RISING);
	exti_enable_request(EXTI17);

	//nvic_enable_irq(NVIC_RTC_IRQ);
	//nvic_set_priority(NVIC_RTC_IRQ, 2);
	
	enter_critical(flags);

    rtc_clear_flag(RTC_SEC);
	rtc_clear_flag(RTC_ALR);
    // rtc_clear_flag(RTC_OW);
	// rtc_interrupt_enable(RTC_SEC);
	rtc_interrupt_enable(RTC_ALR);
	//rtc_interrupt_enable(RTC_OW);
	
	exit_critical(flags);
}


/*********************************************************************
 * Set an alarm n seconds into the future
 *********************************************************************/

void set_alarm(uint8_t sec) 
{
    uint32_t alarm = 0;

	alarm = (rtc_get_counter_val() + sec) & 0xFFFFFFFF;

	rtc_disable_alarm();
	rtc_set_alarm_time(alarm);
	rtc_enable_alarm();
}

