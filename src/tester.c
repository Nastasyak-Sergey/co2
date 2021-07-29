#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <libopencm3/cm3/vector.h>
#include <libopencm3/stm32/f1/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencmsis/core_cm3.h> //
#include <libopencm3/stm32/usart.h>

#include "../inc/board.h"
#include "../inc/common.h"
//#include "../inc/ds18b20.h"
#include "../inc/systick.h"
#include "../inc/serial.h"
#include "../inc/fifo.h"
#include "../inc/i2c.h"
#include "../inc/oled_ssd1306.h"
//#include "../inc/ssd1306_fonts.h"
#include "../inc/errors.h"
#include "../inc/debug.h"
//#include "../inc/backup.h"
//#include "../inc/rtc.h"

//Next code from https://github.com/Mark-271/kitchen-clock-poc
#include "irq.h"
#include "sched.h"
#include "swtimer.h"

//#include "libprintf/printf.h"

#define DEBUG  1
#define GET_CO2_DELAY 5000

static void show_co2(void *param);
static void blink_led(void *param);

//extern uint32_t serial_usart;
static oled_ssd1306_t oled_disp;

static void init (void) {
    int err;
    const struct swtimer_hw_tim hw_tim = {
    	.base = SWTIMER_TIM_BASE,
    	.irq = SWTIMER_TIM_IRQ,
    	.rst = SWTIMER_TIM_RST,
    	.arr = SWTIMER_TIM_ARR_VAL,
    	.psc = SWTIMER_TIM_PSC_VAL,
    };

	struct serial_device serial = {
		.uart = SERIAL_USART,
		.baud = 9600,
		.bits = 8,
		.stopbits = USART_STOPBITS_1,
		.parity = USART_PARITY_NONE,
		.mode = USART_MODE_TX_RX,
		.flow_control = USART_FLOWCONTROL_NONE
	};

	oled_ssd1306_t oled_disp = {
		.i2c = I2C1,
		.addr = SSD1306_I2C_ADDR,
		.x_pos = 0,
		.y_pos = 0,
		.inverted = 0,
		.initialized = 0,
		.display_on = 0
	};

	irq_init();

    board_init();

    sched_init();

	serial_init(&serial);

	err = systick_init();
	if (err) {
		logmsg("Can't initialize systick\n");
		hang();
	}

	ssd1306_init(&oled_disp);


	err = swtimer_init(&hw_tim);
	if (err) {
		logmsg("Can't initialize swtimer\n");
		hang();
	}

    /* Register timer for CO2 sensor */
    int co2_tim_id;

	co2_tim_id = swtimer_tim_register(show_co2, NULL, GET_CO2_DELAY);
	if 	(co2_tim_id < 0) {
		logmsg("Unable to register swtimer for S8\n");
		hang();
	}

    /* Register timer for LED */
   	int led_tim_id;
    led_tim_id = swtimer_tim_register(blink_led, NULL, 500); // ms
	if (led_tim_id < 0) {
		logmsg("Unable to register swtimer for LED\n");
	    hang();
    }

    gpio_set(LED_PORT,LED_PIN);     // PC13 = on
}

static void show_co2(void *param)
{
	UNUSED(param);
//	err_t fifo_err = EOK;
//	unsigned long flags;

	// Request of the CO2 value from Sensair S8 sensor
//	uint8_t buf[] = {0xfe, 0x44, 0, 0x08, 0x02, 0x9f, 0x25};

// Request value of the CO2 and status information from Sensair S8 sensor
// Master Transmit:
//	<FE> <04> <00> <00> <00> <04> <E5> <C6>
// Slave Reply:
//	<FE> <04> <08> <00> <00> <00> <00> <00> <00> <01> <90> <16> <E6>
// 					   | Status  | 				|   CO2   |
 uint8_t buf[] = {0xfe, 0x04, 0, 0, 0, 0x04, 0xe5, 0xc6};

//	uint8_t  s;
//	logmsg("Send through UART1:\n");	
//	for (s = 0; s < ARRAY_SIZE(buf); s++){
//		logmsg("0x%x ", buf[s]);
//	};
//	logmsg("\n");

	serial_send_fifo(serial_usart, buf, ARRAY_SIZE(buf));

	mdelay(100);

	uint8_t rcv[20];
	int8_t rcv_len = 0;
	rcv_len = serial_receive_fifo(serial_usart, rcv, 20);

//	uint8_t s;
//	logmsg("Received from UART1: %d byte\n", rcv_len);
//	for (s = 0; s <= rcv_len;  s++ ){
//		logmsg("0x%x ", rcv[s]);
//	};
//	logmsg("\n");

	uint16_t co2=0;
//	co2 = 256 * rcv[3] + rcv[4]; // In case you read only CO2 value from S8
	co2 = (rcv[9] << 8) + rcv[10];

	uint16_t status;	
	status = (rcv[4] << 8) + rcv[5];


	uint8_t rem, i=0;
	char str[5];

	while (co2) {
			rem = co2 % 10;
			str[i++] = rem + '0';
			co2 /= 10;
	}
   	str[i] = '\0'; // Indicating end of line

	inplace_reverse(str);

	logmsg("CO2 = %s, Status = %d \n", str, status);
	
//	ssd1306_draw_pixel(10, 10, WHITE);
//	ssd1306_draw_pixel(10, 12, WHITE);
//	s = ssd1306_write_char(&oled_disp, '$', font_16x26, WHITE);
 	ssd1306_set_cursor(&oled_disp, 0, 0);
	ssd1306_write_string(&oled_disp, "CO2:", font_16x26, WHITE);
	// Clear previous value of CO2
	ssd1306_write_string(&oled_disp, "     ", font_16x26, WHITE);
 	ssd1306_set_cursor(&oled_disp, 64, 0);
	ssd1306_write_string(&oled_disp, str, font_16x26, WHITE);


	// Status value
 	ssd1306_set_cursor(&oled_disp, 0, 27);
	ssd1306_write_string(&oled_disp, "Status:", font_7x10, WHITE);
 	ssd1306_set_cursor(&oled_disp, 50, 27);
	ssd1306_write_string(&oled_disp, "   ", font_7x10, WHITE);
 	ssd1306_set_cursor(&oled_disp, 50, 27);
	ssd1306_write_string(&oled_disp, ((status==0) ? " Ok": "Err"), font_7x10, WHITE);
	
	ssd1306_update_screen();
}

static void blink_led(void * param) {

    UNUSED(param);
	logmsg("Blink \n");
    gpio_toggle(LED_PORT, LED_PIN);
}


int main(void) {


	init();
	logmsg("Init done\n");

	sched_start();

	while (1) {

    }


}

