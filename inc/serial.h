/**
 *	File contain USART settings and FIFO
 *
 * **/
#ifndef SERIAL_H
#define SERIAL_H

#include "common.h"
#include "errors.h"
#include "fifo.h"
#include <stdint.h>


#define EUSART_NOISE 	BIT(1)
#define EUSART_OVERRUN	BIT(2)
#define EUSART_FRAME	BIT(3)
#define EUSART_PARITY 	BIT(4)

#define UNUSED(x)		((void)x)

extern uint32_t serial_usart;

/* FIFO related definition */
#define FIFO_SIZE 128
fifo_t rx_fifo;
fifo_t tx_fifo;
uint8_t rx_buff[FIFO_SIZE];
uint8_t tx_buff[FIFO_SIZE];





struct serial_device {
	uint32_t uart;
	uint32_t baud;
	uint32_t bits;
	uint32_t stopbits;
	uint32_t parity;
	uint32_t mode;
	uint32_t flow_control;
};


int serial_init(struct serial_device *params);
void serial_exit(void);

int serial_send_fifo(uint32_t serial_usart, uint8_t *buff, fifo_len_t len);

int serial_receive_fifo(uint32_t serial_usart, uint8_t *buff, fifo_len_t len);

#endif /* SERIAL_H */
