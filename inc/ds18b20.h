#ifndef DS18B20_H
#define DS18B20_H

#include <one_wire.h>
#include <stdint.h>

/* Contains parsed data from DS18B20 temperature sensor */
struct ds18b20_temp {
	uint16_t integer:12;
	uint16_t frac;
	char sign;		/* '-' or '+' */
};

struct ds18b20 {
	uint32_t port;
	uint16_t pin;
	struct ds18b20_temp temp;
    uint8_t id[8];
    int timer_id;
};


// state of device
typedef enum{
    DS18_SLEEP,
    DS18_RESETTING,
    DS18_DETECTING,
    DS18_DETDONE,
    DS18_RDYTOSEND,
    DS18_READING,
    DS18_CONVERTIN,
    DS18_ERROR
} ds18b20_state;


int ds18b20_init(struct ds18b20 *obj);
void ds18b20_exit(struct ds18b20 *obj);
struct ds18b20_temp ds18b20_read_temp(struct ds18b20 *obj);
char *ds18b20_temp2str(struct ds18b20_temp *obj, char str[]);


void ds18b20_convert_temp(struct ds18b20 *obj);
struct ds18b20_temp ds18b20_read_temp_by_id(struct ds18b20 *obj, const uint8_t * id);
void ds18b20_read_id(struct ds18b20 * obj);
static int check_crc(const uint8_t data[9]);


#endif /* DRIVERS_DS18B20_H */
