#ifndef RTC_H
#define RTC_H

#include <stdint.h>

typedef void (*rtc_alarm_t)(void);

/* RTC time / date registers */
struct rtc_tm {
	uint8_t ss;
	uint8_t mm;
	uint8_t hh;
	uint8_t day;
	uint8_t date;
	uint8_t month;
	uint8_t year;
};

/* RTC hardware parameters */
struct rtc_device {
    uint32_t clock_source;
    uint32_t prescale_val;
    struct rtc_tm time;
    uint8_t alarm;
    rtc_alarm_t cb;
};


int rtc_init(struct rtc_device *obj);
void set_alarm(uint8_t secs);

#endif
