

#include <ds18b20.h>

#include <one_wire.h>
#include <common.h>
#include <libopencm3/stm32/gpio.h>
#include <stddef.h>
#include <stdint.h>

#include "libprintf/printf.h"

#define TEMPERATURE_CONV_TIME		900

#define CMD_READ_SCRATCHPAD		0xbe

static struct ow ow;

static ds18b20_state ds_state = DS18_SLEEP;

/*
 * thermometer commands
 * send them with bus reset!
 */
// find devices
#define OW_SEARCH_ROM       (0xf0)
// read device (when it is alone on the bus)
#define OW_READ_ROM         (0x33)
// send device ID (after this command - 8 bytes of ID)
#define OW_MATCH_ROM        (0x55)
// broadcast command
#define OW_SKIP_ROM         (0xcc)
// find devices with critical conditions
#define OW_ALARM_SEARCH     (0xec)
/*
 * thermometer functions
 * send them without bus reset!
 */
// start themperature reading
#define OW_CONVERT_T         (0x44)
// write critical temperature to device's RAM
#define OW_SCRATCHPAD        (0x4e)
// read whole device flash
#define OW_READ_SCRATCHPAD   (0xbe)
// copy critical themperature from device's RAM to its EEPROM
#define OW_COPY_SCRATCHPAD   (0x48)
// copy critical themperature from EEPROM to RAM (when power on this operation runs automatically)
#define OW_RECALL_E2         (0xb8)
// check whether there is devices wich power up from bus
#define OW_READ_POWER_SUPPLY (0xb4)


/*
 * thermometer identificator is: 8bits CRC, 48bits serial, 8bits device code (10h)
 * Critical temperatures is T_H and T_L
 * T_L is lowest allowed temperature
 * T_H is highest -//-
 * format T_H and T_L: 1bit sigh + 7bits of data
 */

/*
 * RAM register:
 * 0 - themperature: 1 ADU == 0.5 degrC
 * 1 - sign (0 - T>0 degrC ==> T=byte0; 1 - T<0 degrC ==> T=byte0-0xff+1)
 * 2 - T_H
 * 3 - T_L
 * 4 - 0xff (reserved)
 * 5 - 0xff (reserved)
 * 6 - COUNT_REMAIN (0x0c)
 * 7 - COUNT PER DEGR (0x10)
 * 8 - CRC
 */


/**
 * Parse temperature register from DS18B20.
 *
 * @param lsb Least significant byte of temperature register
 * @param msb Most significant byte of temperature register
 * @return Parsed value
 */
static struct ds18b20_temp ds18b20_parse_temp(uint8_t lsb, uint8_t msb)
{
	struct ds18b20_temp tv;

	tv.integer = (msb << 4) | (lsb >> 4);
	if (msb & BIT(7)) {
		tv.sign = '-';
		/*
		 * Handle negative 2's complement frac value:
		 *   1. Take first 4 bits from LSB (negative part)
		 *   2. Append missing 1111 bits (0xf0), to be able to invert
		 *      8-bit variable
		 *   3. -1 and invert (to handle 2's complement format),
		 *      accounting for implicit integer promotion rule (by
		 *      casting result to uint8_t)
		 */
		tv.frac = 625 * (uint8_t)(~(((lsb & 0xf) - 1) | 0xf0));
		/* Handle negative 2's complement integer value */
		tv.integer = ~tv.integer;
		if (tv.frac == 0)
			tv.integer++;
	} else {
		tv.sign = '+';
		tv.frac = 625 * (lsb & 0xf);
	}

	return tv;
}


/**
 * Send command to all DS18B20 sensors conevert temperature.
 *
 * @param obj 1-wire device object
 * @return Parsed value
 */
void ds18b20_convert_temp(struct ds18b20 *obj)
{

	ow_reset_pulse(&ow);
	ow_write_byte(&ow, OW_SKIP_ROM);
	ow_write_byte(&ow, OW_CONVERT_T);
    ds_state = DS18_CONVERTIN;	
}

/**
 * Read temperature register from DS18B20.
 *
 * @param obj 1-wire device object
 * @return Parsed value
 */
struct ds18b20_temp ds18b20_read_temp(struct ds18b20 *obj)
{
	unsigned long flags;
	uint8_t data[2];
	size_t i;

    ow_reset_pulse(&ow);
	ow_write_byte(&ow, OW_SKIP_ROM);
	ow_write_byte(&ow, OW_CONVERT_T);
	
    enter_critical(flags);
	mdelay(TEMPERATURE_CONV_TIME); // 900ms :)
	exit_critical(flags);

    ow_reset_pulse(&ow);
	ow_write_byte(&ow, OW_SKIP_ROM);
	ow_write_byte(&ow, OW_READ_SCRATCHPAD);

	for (i = 0; i < 2; i++)
		data[i] = ow_read_byte(&ow);
	ow_reset_pulse(&ow);

	obj->temp = ds18b20_parse_temp(data[0], data[1]);
	return obj->temp;
}

/**
 * Read ds18b20 ROM ID
 *
 * @param obj Contains parsed temperature register from DS18B20
 * @return
 */
void ds18b20_read_id(struct ds18b20 * obj)
{
    size_t i;

	ow_reset_pulse(&ow);
	ow_write_byte(&ow, OW_READ_ROM); // 33h
    for (i = 0; i < 8; i++)
        obj->id[i] = ow_read_byte(&ow);
}


/**
 * Check CRC of the 9 byte msg
 *
 * @param input msg
 * @return 1 if msg CRC match last byte
 */

static int check_crc(const uint8_t data[9]){
    uint8_t crc = 0;
    for(int n = 0; n < 8; ++n){
        crc = crc ^ data[n];
           for(int i = 0; i < 8; i++){
               if(crc & 1)
                   crc = (crc >> 1) ^ 0x8C;
               else
                   crc >>= 1;
           }
    }
    if(data[8] == crc) return 0;
    return 1;
}

/**
 * Read temperature register from DS18B20 by ID.
 *
 * @param obj 1-wire device object
 * @return Parsed value
 */
struct ds18b20_temp ds18b20_read_temp_by_id(struct ds18b20 *obj, const uint8_t *id )
{
	unsigned long flags;
	int8_t data[9];
	size_t i;
    switch(ds_state) {
        case DS18_CONVERTIN:

            ow_reset_pulse(&ow);
    	    ow_write_byte(&ow, OW_MATCH_ROM);
            for(i = 0; i < 8; ++i)
                ow_write_byte(&ow, (*(id + i)));

            ow_write_byte(&ow, OW_READ_SCRATCHPAD);

    	    for (i = 0; i < 9; i++)
    	    	data[i] = ow_read_byte(&ow);

            ow_reset_pulse(&ow);
            // printf("CRC %s \n", check_crc(data[0])? "Ok" : "Nok" );
    	    obj->temp = ds18b20_parse_temp(data[0], data[1]);
            return obj->temp;
        break;
        default:
            ds_state = DS18_SLEEP;
            return obj->temp;
        }
}


/**
 * Convert temperature data to null-terminated string.
 *
 * @param obj Contains parsed temperature register from DS18B20
 * @param str Array to store string literal
 * @return Pointer to @ref str
 */
char *ds18b20_temp2str(struct ds18b20_temp *obj, char str[])
{
	int i = 0;
	uint16_t rem;

	if (!obj->frac) {
		str[i++] = '0';
	} else {
		while (obj->frac) {
			rem = obj->frac % 10;
			str[i++] = rem + '0';
			obj->frac /= 10;
		}
	}
	str[i++] = '.';
	if (!obj->integer) {
		str[i++] = '0';
	} else {
		while (obj->integer) {
			rem = obj->integer % 10;
			str[i++] = rem + '0';
			obj->integer /= 10;
		}
	}
	str[i++] = obj->sign;
	str[i] = '\0';
	inplace_reverse(str);

	return str;
}

int ds18b20_init(struct ds18b20 *obj)
{
	ow.port = obj->port;
	ow.pin = obj->pin;

	return ow_init(&ow);
}

/* Destroy ds18b20 object */
void ds18b20_exit(struct ds18b20 *obj)
{
	UNUSED(obj);
	ow_exit(&ow);
}
