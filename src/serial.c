/**
 * 
 * @file
 *
 *	File contain USART settings and FIFO
 *
 *
 */
#include "../inc/errors.h"
#include "../inc/serial.h"
#include "../inc/common.h"
#include "../inc/fifo.h"
#include "../inc/irq.h"
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/nvic.h>
#include "libprintf/printf.h"

#include <stdio.h>
#include <stdint.h>
#include <errno.h>

/* singleton object */
uint32_t serial_usart;

#define USART_IRQS 1 // Number of IRQs

/**
 * IRQ handler for USART1
 *
 *
 * **/
static irqreturn_t usart1_isr_handler(int irq, void *data)
{
//TODO: struct rtc *obj = (struct rtc *)(data);

	UNUSED(irq);
	UNUSED(data);

	int16_t status;
	uint8_t rx_data;
	uint8_t tx_data;
	int8_t tx_data_len;
	int8_t rx_last_error = 0;

	status = USART_SR(serial_usart);
	rx_last_error = 0;

	// Check if we were called because of RXNE
	if(status & USART_SR_RXNE){
		// parse incoming byte
		rx_data = usart_recv(serial_usart);

//		printf("From RX ISR data = %x \n ", rx_data);
		// Check flag of the USART NE, ORE, FE, PE
		if (status & USART_SR_NE) {
			rx_last_error |= EUSART_NOISE;
		} else if (status & USART_SR_ORE) {
			rx_last_error |= EUSART_OVERRUN;
		} else if (status & USART_SR_FE) {
			rx_last_error |= EUSART_FRAME;
		} else if( status & USART_SR_PE) {
			rx_last_error |= EUSART_PARITY;
		}

		rx_fifo.last_error = rx_last_error;
			// Put new data in to RX FIFO buffer
			// TODO add error checks of FIFO 
		fifo_put(&rx_fifo, &rx_data, 1);
	}
	// Check if we were called because of TXE -> send next byte in buffer
	if((USART_CR1(serial_usart) & USART_CR1_TXEIE) && (status & USART_SR_TXE)){

		// Get new data from TX FIFO
		tx_data_len  = fifo_get(&tx_fifo, &tx_data, 1);
	
		if (tx_data_len > 0) {

			usart_send(serial_usart, tx_data);
		//	printf("From TX ISR data = %x \n ", tx_data);

		} else {

			usart_disable_tx_interrupt(serial_usart);

		}
	}

	return IRQ_HANDLED;
}

//	Structure where we define descriotion od USART IRQs
static struct irq_action usart_irq_act[USART_IRQS] = {
	{
		.handler = usart1_isr_handler,
		.irq = NVIC_USART1_IRQ,
		.name = "usart1_isr",
	},
/*	{
		.handler = usart2_isr_handler,
		.irq = NVIC_USART2_IRQ,
		.name = "usart2_isr,",
	}*/
};


int serial_init(struct serial_device *obj)
{
	int ret;
	unsigned long flags;
	err_t fifo_err = EOK;

	serial_usart = obj->uart;

	/* Initialize FIFOs for Rx and Tx */
	fifo_err = fifo_init(&rx_fifo, rx_buff, FIFO_SIZE);
	if (fifo_err != EOK) {
		printf("Can't init RX FIFO \n");
	}

	fifo_err = fifo_init(&tx_fifo, tx_buff, FIFO_SIZE);
   	if (fifo_err != EOK) {
		printf("Can't init TX FIFO \n");
	}

	/* Register interrupt handlers */
	int i;
    for (i = 0; i < USART_IRQS; i++) {
		usart_irq_act[i].data = (void *)obj; // PUT FIFO here
		ret = irq_request(&usart_irq_act[i]);
		if (ret) {
            printf("Unable request USART IRQ\n");
			return ret;
        }
	}

	nvic_enable_irq(NVIC_USART1_IRQ);
	usart_set_baudrate(serial_usart, obj->baud);
	usart_set_databits(serial_usart, obj->bits);
	usart_set_stopbits(serial_usart, obj->stopbits);
	usart_set_parity(serial_usart, obj->parity);
	usart_set_mode(serial_usart, obj->mode);
	usart_set_flow_control(serial_usart, obj->flow_control);
	usart_enable_rx_interrupt(serial_usart);

	usart_enable(serial_usart);

	return 0;
}

int serial_send_fifo(uint32_t serial_usart, uint8_t *buff, fifo_len_t len)
{

	err_t fifo_err = EOK;

	fifo_err = fifo_put(&tx_fifo, buff, len);
	if (fifo_err != EOK) {
		printf("Unnable to put in to tx_fifo \n");
		return -1;
	}

	usart_enable_tx_interrupt(serial_usart);

	return len;
}


int serial_receive_fifo(uint32_t serial_usart, uint8_t *buff, fifo_len_t len)
{
	int32_t rcv_len;
	rcv_len = fifo_get(&rx_fifo, buff, len);

	if (0 > rcv_len) {
		printf("Unnable to get data from rx_fifo\n");
		return -1;
	}
	return rcv_len;
}

void serial_exit(void)
{
	usart_disable(serial_usart);
	serial_usart = 0;
}
