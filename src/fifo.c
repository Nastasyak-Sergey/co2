#include "../inc/fifo.h"
#include "../inc/errors.h"


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


static inline fifo_len_t _fifo_numleft(fifo_t *fifo)
{
	// Nleft = (buflen - D) % buflen  or use      & (fifo->bufflen - 1)
	fifo_len_t left = ((fifo->buflen - fifo->wridx) + fifo->rdidx) & (fifo->buflen - 1);
	// when wrptr == rdptr, we have either full or empty
	// formula above gives 0 left, so fix this manually
	if (0 == left)
		return (fifo->isfull) ? 0 : fifo->buflen;

	return left;
}


// Dynamically init fifo
err_t fifo_init(fifo_t *fifo, uint8_t *buf, fifo_len_t buflen)
{
	if ((NULL == fifo) || (NULL == buf) || (0 == buflen))
		return EWRONGARG;

	FIFO_DECLARE(tmp, buf, buflen);
	*fifo = tmp;	// copy
	return EOK;
}


// Put N bytes into BFIFO
err_t fifo_put(fifo_t *fifo, uint8_t *barr, uint32_t len)
{
	unsigned long flags;

	if ((NULL == fifo) || (NULL == barr))
		return EUNKNOWN;

	// item can not be put at all -- it is a different kind of error
	if (len > fifo->buflen)
		return ERANGE;

	fifo_len_t left = _fifo_numleft(fifo);

	// Our buffer is all-or-nothing. We either put array in it, or return error. No partial writes
	if (left < len)
		return EFULL;

	// Now we know there's enough space for elements we're trying to put. Do it

	// first put elements, only then update index
	// this (hopefully) will allow for lock-free use in one producer - one consumer case
	for (fifo_len_t i = 0; i < len; i++) {
		enter_critical(flags);
		fifo->buf[(fifo->wridx + i) & (fifo->buflen - 1)] = barr[i];
		exit_critical(flags);
	}

	enter_critical(flags);
	fifo_len_t newwr = (fifo->wridx + len) & (fifo->buflen - 1);
	exit_critical(flags);

	if (newwr == fifo->rdidx)
		fifo->isfull = true;

	// memory barrier here

	// update index
	fifo->wridx = newwr;
	return EOK;
}


// Try getting up to N bytes from BFIFO
int32_t fifo_get(fifo_t *fifo, uint8_t *barr, uint32_t len)
{
	unsigned long flags;

	if ((NULL == fifo) || (NULL == barr))
		return EUNKNOWN;

	// trying to read 0 elements -- return 0 read to be consistent with api users
	if (0 == len)
		return EEMPTY;

	fifo_len_t used = fifo->buflen - _fifo_numleft(fifo);

	if (0 == used)
		return EEMPTY;		// can not read from empty buffer

	// truncate length as we're returning number of elements actually read -- to simplify algo
	len = (len > used) ? used : len;

	// perform actual read
	for (fifo_len_t i = 0; i < len; i++) {
		enter_critical(flags);
		barr[i] = fifo->buf[(fifo->rdidx + i) & (fifo->buflen - 1)];
		exit_critical(flags);
	}

	// now update the pointer
	enter_critical(flags);
	fifo->rdidx = (fifo->rdidx + len) & (fifo->buflen - 1);
	exit_critical(flags);
	// if we came here -- it means we have successfully read something
	// so buffer can not be full -- remove flag
	fifo->isfull = false;

	return len;		// number of elements actually read
}

