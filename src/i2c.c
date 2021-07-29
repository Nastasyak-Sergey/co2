/**
 * @file
 *
 * I2C controller driver.
 *
 * Features:
 *   - right now works only for transfer
 *   - only poll mode is implemented
 */

#include "../inc/i2c.h"
#include "../inc/common.h"
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>

#include <errno.h>
#include <stddef.h>

#include "libprintf/printf.h"
/* Timeout values */
#define I2C_TIMEOUT_FLAG    35	/* wait for generic flag, msec */
#define I2C_TIMEOUT_BUSY	25	/* wait for BUSY flag, msec */

/*
 * I2C states for driver internal usage.
 *
 * Values are CMSIS-compatible. Those are *not* registers flags.
 *
 * b7-b6 - Error information
 *   00: No Error
 *   01: Abort (Abort user request ongoing)
 *   10: Timeout
 *   11: Error
 * b5 - IP initialization status
 *   0: Reset (IP not initialized)
 *   1: Init done (IP initialized and ready to use. i2c_init() function called)
 * b4 - (not used)
 *   x: Should be set to 0
 * b3
 *   0: Ready or Busy (No Listen mode ongoing)
 *   1: Listen (IP in Address Listen Mode)
 * b2 - Intrinsic process state
 *   0: Ready
 *   1: Busy (IP busy with some configuration or internal operations)
 * b1 - Rx state
 *   0: Ready (no Rx operation ongoing)
 *   1: Busy (Rx operation ongoing)
 * b0 - Tx state
 *   0: Ready (no Tx operation ongoing)
 *   1: Busy (Tx operation ongoing)
 */
#define I2C_STATE_RESET			0x00
#define I2C_STATE_READY			BIT(5)
#define I2C_STATE_BUSY			(BIT(5) | BIT(2))
#define I2C_STATE_BUSY_TX		(BIT(5) | BIT(0))
#define I2C_STATE_BUSY_RX		(BIT(5) | BIT(1))
#define I2C_STATE_LISTEN		(BIT(5) | BIT(3))
#define I2C_STATE_BUSY_TX_LISTEN	(BIT(5) | BIT(3) | BIT(0))
#define I2C_STATE_BUSY_RX_LISTEN	(BIT(5) | BIT(3) | BIT(1))
#define I2C_STATE_ABORT			(BIT(5) | BIT(6))
#define I2C_STATE_TIMEOUT		(BIT(5) | BIT(7))
#define I2C_STATE_ERROR			(BIT(5) | BIT(6) | BIT(7))

/*
 * I2C error codes for driver internal usage.
 *
 * Values are CMSIS-compatible. Those are *not* registers flags.
 */
#define I2C_ERROR_NONE		0x00	/* no errors */
#define I2C_ERROR_BERR		BIT(0)	/* bus error */
#define I2C_ERROR_ARLO		BIT(1)	/* arbitration lost error */
#define I2C_ERROR_AF		BIT(2)	/* acknowledge failure */
#define I2C_ERROR_OVR		BIT(3)	/* overrun/underrun error */
#define I2C_ERROR_DMA		BIT(4)	/* DMA transfer error */
#define I2C_ERROR_TIMEOUT	BIT(5)	/* timeout error */

struct i2c_t {
	uint32_t base;		/* I2C register base address */
	uint32_t state;		/* state of current I2C transaction */
	uint32_t error;		/* errors happened during last transaction */
};

static struct i2c_t i2c;

static void i2c_setup(uint32_t base)
{
	/* Disable the I2C before changing any configuration */
	i2c_peripheral_disable(base);

	/* Configure I2C for 100 kHz */
//	i2c_set_speed(base, i2c_speed_sm_100k, rcc_apb1_frequency / 1e6);
	/* Configure I2C for 400 kHz */
	i2c_set_speed(base, i2c_speed_fm_400k, rcc_apb1_frequency / 1e6);
	/* Everything is configured, enable the peripheral */
	i2c_peripheral_enable(base);
}

/**
 * Send START condition and slave address on I2C bus.
 *
 * @param addr Slave device 7-bit address
 * @param rw Either I2C_WRITE for write transaction, or I2C_READ for read
 * @return 0 on success or negative value on error
 */
static int i2c_send_start_addr_poll(uint8_t addr, uint8_t rw)
{
	int ret;

	i2c_send_start(i2c.base);
	// SB Start bit, Set when a Start condition generated. 
	// MSL: Master/slave 0: Slave Mode, 1: Master Mode
	// BUSY: Bus busy 0: No communication on the bus, 1: Communication ongoing on the EBUSY

	//ret = wait_event_timeout((I2C_SR1(i2c.base) & I2C_SR1_SB), 
	//						  I2C_TIMEOUT_FLAG);
	ret = wait_event_timeout((I2C_SR1(i2c.base) & I2C_SR1_SB) & 
							 (I2C_SR2(i2c.base) & (I2C_SR2_MSL | I2C_SR2_BUSY)), 
							 I2C_TIMEOUT_FLAG);
	if (ret != 0) {
		printf("Send start, ERR TIMEOUT \n");
		goto err_timeout;
	}

	i2c_send_7bit_address(i2c.base, addr, rw);

	// ADDR Address sent (master mode) 	0: No end of address transmission
	//									1: End of address transmission
	ret = wait_event_timeout(I2C_SR1(i2c.base) & I2C_SR1_ADDR, I2C_TIMEOUT_FLAG);

	// Check Slave AK
	// AF Acknowledge failure NAK
	if (I2C_SR1(i2c.base) & I2C_SR1_AF) {
		printf("Send addr 0x%x, ERR NAK, ret = %x\n", addr, ret);
		goto err_nack;
	}
	if (ret != 0) {
		printf("Send addr 0x%x, ERR TIMEOUT, ret = %x\n", addr, ret);
  		goto err_timeout;
	}

	(void)I2C_SR2(i2c.base); /* clear ADDR flag (SR1 already read above) */

	/* Wait for TxE = 1 to be set by hardware in response to ADDR */
	if (rw == I2C_WRITE) {
		ret = wait_event_timeout(I2C_SR1(i2c.base) & I2C_SR1_TxE, I2C_TIMEOUT_FLAG);
		if (ret != 0) {
			printf("Wait TxE ERR TIMEOUT \n");
			goto err_timeout;
		}
	}

	if (I2C_SR1(i2c.base) & I2C_SR1_AF) {
		printf("Send addr 0x%x, ERR NAK \n", addr);
		goto err_nack;
	}

	return 0;

err_nack:
	i2c_send_stop(i2c.base);
	I2C_SR1(i2c.base) = ~I2C_SR1_AF;
	WRITE_ONCE(i2c.error, I2C_ERROR_AF);
	WRITE_ONCE(i2c.state, I2C_STATE_READY);
	return -EIO;

err_timeout:
	WRITE_ONCE(i2c.error, I2C_ERROR_TIMEOUT);
	WRITE_ONCE(i2c.state, I2C_STATE_READY);
	return -ETIMEDOUT;
}

/**
 * Send one byte on the bus.
 *
 * @param data Byte to send
 * @return 0 on success or negative value on error
 */
static int i2c_send_byte_poll(uint8_t data)
{
	int ret;

	i2c_send_data(i2c.base, data);
	/*
	 * Poll on BTF to receive data because in polling mode we can not
	 * guarantee the EV8 software sequence is managed before the current
	 * byte transfer completes.
	 */
	 //BTF: Byte transfer finished
	ret = wait_event_timeout(I2C_SR1(i2c.base) & I2C_SR1_BTF,
				 I2C_TIMEOUT_FLAG);
	if (I2C_SR1(i2c.base) & I2C_SR1_AF)
		goto err_nack;
	if (ret != 0)
		goto err_timeout;

	return 0;

err_nack:
	i2c_send_stop(i2c.base);
	I2C_SR1(i2c.base) = ~I2C_SR1_AF;
	WRITE_ONCE(i2c.error, I2C_ERROR_AF);
	WRITE_ONCE(i2c.state, I2C_STATE_READY);
	return -EIO;

err_timeout:
	WRITE_ONCE(i2c.error, I2C_ERROR_TIMEOUT);
	WRITE_ONCE(i2c.state, I2C_STATE_READY);
	return -ETIMEDOUT;
}

/**
 * Send buffer of bytes on bus.
 *
 * @param buf Buffer of bytes to send
 * @param len Buffer length (bytes count)
 * @return 0 on success or negative value on error
 */
static int i2c_send_buf_poll(const uint8_t *buf, uint16_t len)
{
	uint16_t i;

	for (i = 0; i < len; ++i) {
		int ret;

		ret = i2c_send_byte_poll(buf[i]);
		if (ret != 0)
			return ret;
	}

	return 0;
}

/**
 * Receive buffer of bytes on the bus.
 *
 * @param[out] buf Buffer to store received bytes
 * @param[in] len Buffer length
 * @return 0 on success or negative value on error
 */
static int i2c_receive_buf_poll(uint8_t *buf, uint16_t len)
{
	size_t i;
	int ret;

	for (i = 0; i < len; len--) {
		if (len == 3)
			break;
		ret = wait_event_timeout(I2C_SR1(i2c.base) & I2C_SR1_BTF,
				I2C_TIMEOUT_FLAG);
		if (ret != 0)
			goto err_timeout;
		buf[i++] = i2c_get_data(i2c.base);
	}

	ret = wait_event_timeout(I2C_SR1(i2c.base) & I2C_SR1_BTF,
				 I2C_TIMEOUT_FLAG);
		if (ret != 0)
			goto err_timeout;

	i2c_disable_ack(i2c.base);
	buf[i++] = i2c_get_data(i2c.base);

	i2c_send_stop(i2c.base);
	buf[i++] = i2c_get_data(i2c.base);

	ret = wait_event_timeout(I2C_SR1(i2c.base) & I2C_SR1_RxNE,
				 I2C_TIMEOUT_FLAG);
		if (ret != 0)
			goto err_timeout;
	buf[i] = i2c_get_data(i2c.base);

	/* Make sure that the STOP bit is cleared by hardware */
	ret = wait_event_timeout((I2C_CR1(i2c.base) & I2C_CR1_STOP) == 0,
				 I2C_TIMEOUT_FLAG);
	if (ret != 0)
		goto err_timeout;

	i2c_enable_ack(i2c.base);

	WRITE_ONCE(i2c.state, I2C_STATE_READY);

	return 0;

err_timeout:
	WRITE_ONCE(i2c.error, I2C_ERROR_TIMEOUT);
	WRITE_ONCE(i2c.state, I2C_STATE_READY);
	return -ETIMEDOUT;
}

/**
 * Send STOP condition on I2C bus.
 *
 * @return 0 on success or negative value on failure
 */
static int i2c_send_stop_poll(void)
{
	int ret;

	/* We already waited for BTF=1 in i2c_send_byte_poll() */
	i2c_send_stop(i2c.base);

	/* Make sure that the STOP bit is cleared by hardware */
	ret = wait_event_timeout((I2C_CR1(i2c.base) & I2C_CR1_STOP) == 0,
				 I2C_TIMEOUT_FLAG);
	if (ret != 0)
		goto err_timeout;

	return 0;

err_timeout:
	WRITE_ONCE(i2c.error, I2C_ERROR_TIMEOUT);
	WRITE_ONCE(i2c.state, I2C_STATE_READY);
	return -ETIMEDOUT;
}

/* -------------------------------------------------------------------------- */

/**
 * Write buffer of data to I2C slave device using polling mode (no DMA, no IRQ).
 *
 * This function is synchronous, implements waiting events by polling registers.
 *
 * Possible errors:
 *   -EBUSY: previous I2C transaction is not finished
 *   -ETIMEDOUT: timeout happened during I2C transaction
 *   -EIO: I/O error during I2C transaction
 *
 * @param addr Slave device I2C address
 * @param reg I2C register address in slave device
 * @param buf Buffer of data to write
 * @param len Buffer size, in bytes
 * @return 0 on success or negative value on failure
 */
int i2c_write_buf_poll(uint8_t addr, uint8_t reg, const uint8_t *buf,
		       uint16_t len)
{
	int ret;

	cm3_assert(len > 0);

	if (READ_ONCE(i2c.state) != I2C_STATE_READY)
		return -EBUSY;

	if (wait_event_timeout((I2C_SR2(i2c.base) & I2C_SR2_BUSY) == 0,
			       I2C_TIMEOUT_BUSY)) {
		return -EBUSY;
	}

	WRITE_ONCE(i2c.state, I2C_STATE_BUSY_TX);
	WRITE_ONCE(i2c.error, I2C_ERROR_NONE);

	ret = i2c_send_start_addr_poll(addr, I2C_WRITE);
	if (ret != 0)
		return ret;

	ret = i2c_send_byte_poll(reg);
	if (ret != 0)
		return ret;

	ret = i2c_send_buf_poll(buf, len);
	if (ret != 0)
		return ret;

	ret = i2c_send_stop_poll();
	if (ret != 0)
		return ret;

	WRITE_ONCE(i2c.state, I2C_STATE_READY);

	return 0;
}

/**
 * Read single byte from I2C slave device using polling mode.
 *
 * @param[in] addr Slave device I2C address
 * @param[in] reg I2C register address in slave device
 * @param[out] data Variable to store data
 * @return 0 on success or negative value on error
 */
int i2c_read_single_byte_pol(uint8_t addr, uint8_t reg, uint8_t *data)
{
	int ret;

	if (READ_ONCE(i2c.state) != I2C_STATE_READY)
		return -EBUSY;

	if (wait_event_timeout((I2C_SR2(i2c.base) & I2C_SR2_BUSY) == 0,
			       I2C_TIMEOUT_BUSY)) {
		return -EBUSY;
	}

	WRITE_ONCE(i2c.state, I2C_STATE_BUSY_RX);
	WRITE_ONCE(i2c.error, I2C_ERROR_NONE);

	ret = i2c_send_start_addr_poll(addr, I2C_WRITE);
	if (ret != 0)
		return ret;

	ret = i2c_send_byte_poll(reg);
	if (ret != 0)
		return ret;

	i2c_send_start(i2c.base);
	if (wait_event_timeout(I2C_SR1(i2c.base) & I2C_SR1_SB,
			       I2C_TIMEOUT_FLAG)) {
		goto err_timeout;
	}

	i2c_send_7bit_address(i2c.base, addr, I2C_READ);
	ret = wait_event_timeout(I2C_SR1(i2c.base) & I2C_SR1_ADDR,
				 I2C_TIMEOUT_FLAG);
	if (ret != 0)
		goto err_timeout;

	/* Generate EV6_3 event */
	i2c_disable_ack(i2c.base);
	(void)I2C_SR2(i2c.base); /* Clear ADDR flag */
	i2c_send_stop(i2c.base);

	/* Wait for data register is not empty (RxNE = 1) */
	ret = wait_event_timeout(I2C_SR1(i2c.base) & I2C_SR1_RxNE,
				 I2C_TIMEOUT_FLAG);
	if (ret != 0)
		goto err_timeout;

	*data = i2c_get_data(i2c.base);

	ret = wait_event_timeout((I2C_CR1(i2c.base) & I2C_CR1_STOP) == 0,
				 I2C_TIMEOUT_FLAG);
	if (ret != 0)
		goto err_timeout;

	i2c_enable_ack(i2c.base);

	WRITE_ONCE(i2c.state, I2C_STATE_READY);

	return 0;

err_timeout:
	WRITE_ONCE(i2c.error, I2C_ERROR_TIMEOUT);
	WRITE_ONCE(i2c.state, I2C_STATE_READY);
	return -ETIMEDOUT;
}

/**
 * Read n bytes of data into the buffer from I2C slave device using polling
 * mode (no DMA, no IRQ).
 *
 * This function is synchronous, implements waiting events by polling registers.
 *
 * Possible errors:
 *   -EBUSY: previous I2C transaction is not finished
 *   -ETIMEDOUT: timeout happened during I2C transaction
 *   -EIO: I/O error during I2C transaction
 *
 * @param addr Slave device I2C address
 * @param reg I2C register address in slave device
 * @param[out] buf Buffer of data to read in
 * @param len Buffer size, in bytes
 * @return 0 on success or negative value on failure
 */
int i2c_read_buf_poll(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
	int ret;

	cm3_assert(len > 2);

	if (READ_ONCE(i2c.state) != I2C_STATE_READY)
		return -EBUSY;

	if (wait_event_timeout((I2C_SR2(i2c.base) & I2C_SR2_BUSY) == 0,
			       I2C_TIMEOUT_BUSY)) {
		return -EBUSY;
	}

	WRITE_ONCE(i2c.state, I2C_STATE_BUSY_RX);
	WRITE_ONCE(i2c.error, I2C_ERROR_NONE);

	ret = i2c_send_start_addr_poll(addr, I2C_WRITE);
	if (ret != 0)
		return ret;

	ret = i2c_send_byte_poll(reg);
	if (ret != 0)
		return ret;

	ret = i2c_send_start_addr_poll(addr, I2C_READ);
	if (ret != 0)
		return ret;

	ret = i2c_receive_buf_poll(buf, len);
	if (ret != 0)
		return ret;

	WRITE_ONCE(i2c.state, I2C_STATE_READY);

	return 0;
}

/**
 * Check if slave is present on the bus.
 *
 * @param addr Slave device I2C address
 * @return 0 if slave is present or negative value on error
 */
int i2c_detect_device(uint8_t addr)
{
	int ret;

	if (READ_ONCE(i2c.state) != I2C_STATE_READY)
		return -EBUSY;

	//if (wait_event_timeout((I2C1_SR2 & I2C_SR2_BUSY) == 0, I2C_TIMEOUT_BUSY)) {
	if (wait_event_timeout((I2C_SR2(i2c.base) & I2C_SR2_BUSY) == 0, I2C_TIMEOUT_BUSY)) {
		printf("Wait timeout I2C busy\n");
		return -EBUSY;
	}

	WRITE_ONCE(i2c.state, I2C_STATE_BUSY);
	WRITE_ONCE(i2c.error, I2C_ERROR_NONE);

	ret = i2c_send_start_addr_poll(addr, I2C_WRITE);
	if (ret != 0) {
		return ret;
	}
	ret = i2c_send_stop_poll();
	if (ret != 0) {
		return ret;
	}

	if (wait_event_timeout((I2C_SR2(i2c.base) & I2C_SR2_BUSY) == 0,
			       I2C_TIMEOUT_BUSY)) {
		return -EBUSY;
	}

	WRITE_ONCE(i2c.state, I2C_STATE_READY);
	return 0;
}

/**
 * Initialize I2C module.
 *
 * @param base I2C register base address, e.g. I2C1
 */
int i2c_init(uint32_t base)
{
	i2c_setup(base);

	i2c.base = base;
	i2c.state = I2C_STATE_READY;
	i2c.error = I2C_ERROR_NONE;

	return 0;
}
