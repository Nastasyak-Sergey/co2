#ifndef BOARD_H
#define BOARD_H


#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/usart.h>


/* Vector table size (=sizeof(vector_table) */
#define CONFIG_VTOR_SIZE		0x150

/* GPIO level transient time, usec */
#define CONFIG_GPIO_STAB_DELAY		10

/* Serial port  */
#define SERIAL_GPIO_PORT	GPIOA
#define SERIAL_GPIO_TX_PIN	GPIO_USART1_TX
#define SERIAL_GPIO_RX_PIN  GPIO_USART1_RX
#define SERIAL_USART		USART1
#define SERIAL_USART_RCC	RCC_USART1
#define SERIAL_GPIO_RCC		RCC_GPIOA


/* I2C for OLED display */
#define OLED_I2C			I2C1
#define I2C_GPIO_PORT		GPIOB
//#define 	GPIO_I2C1_RE_SCL   GPIO8 /* PB8 */
//#define 	GPIO_I2C1_RE_SDA   GPIO9 /* PB9 */
#define I2C_SCL_PIN			GPIO_I2C1_RE_SCL
#define I2C_SDA_PIN			GPIO_I2C1_RE_SDA
#define I2C_GPIO_RCC		RCC_GPIOB
#define I2C_RCC 			RCC_I2C1

/* LED on board */
#define LED_RCC             RCC_GPIOC
#define LED_PORT            GPIOC
#define LED_PIN             GPIO13

/* USB */
#define USB_RCC             RCC_USB
#define USB_GPIO_RCC        RCC_GPIOA
#define USB_PORT            GPIOA
#define USB_DP_PIN          GPIO12
#define USB_DN_PIN          GPIO11

/* Temperature sensor */
#define DS18B20_GPIO_RCC	RCC_GPIOB
#define DS18B20_GPIO_PORT	GPIOB
#define DS18B20_GPIO_PIN	GPIO10

/* General purpose timer */
#define SWTIMER_TIM_RCC		RCC_TIM2
#define SWTIMER_TIM_BASE	TIM2
#define SWTIMER_TIM_IRQ		NVIC_TIM2_IRQ
#define SWTIMER_TIM_RST		RST_TIM2
#define SWTIMER_TIM_ARR_VAL	5000-1 // 19999
#define SWTIMER_TIM_PSC_VAL	36-1    //5


int board_init(void);

#endif /* BOARD_H */
