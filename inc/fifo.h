#ifndef FIFO_H
#define FIFO_H

#include "errors.h"
#include "common.h"

#include <stdint.h>

typedef uint16_t fifo_len_t;

struct fifo {
	uint8_t *buf;
	fifo_len_t buflen;
	fifo_len_t rdidx;
	fifo_len_t wridx;
	fifo_len_t last_error;
	uint8_t isfull : 1;
};

typedef struct fifo fifo_t;



#define FIFO_INIT(_buf, _buflen) (\
	(fifo_t) {					  		 \
		.buf = (_buf),				  		 \
		.buflen = (_buflen),		  		 \
		.rdidx = 0u,				  		 \
		.wridx = 0u,				  		 \
		.last_error = 0u,					 \
		.isfull = false				  		 \
	})


/**
 * Statically declare and initialize fifo
 * @name: name under which `fifo_t` fifo object will be available
 * @buf: buffer used for bfifo
 * @buflen: length of the passed `buf` buffer
 */
#define FIFO_DECLARE(name, buf, buflen) fifo_t name = FIFO_INIT(buf, buflen)


/**
 * Dynamically initialize bfifo
 * @buf: buffer used for bfifo
 * @buflen: length of the passed `buf` buffer
 */
err_t fifo_init(fifo_t *fifo, uint8_t *buf, fifo_len_t buflen);

err_t fifo_put(fifo_t *fifo, uint8_t *barr, uint32_t len);

int32_t fifo_get(fifo_t *fifo, uint8_t *barr, uint32_t len);

#endif
