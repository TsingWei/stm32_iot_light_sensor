/* Includes -----------------------------------------------------------------*/
#include "fifo.h"
#include "stdlib.h"
#include "string.h"
// #include "includes.h"

/* Variables -----------------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/******************************************************************************/

/*
 * internal helper to calculate the unused elements in a fifo
 */
static __inline unsigned int fifo_unused(struct fifo *fifo)
{
  return (fifo->mask + 1) - (fifo->in - fifo->out);
}

unsigned int fifo_used(struct fifo *fifo)
{
  return (fifo->in - fifo->out);
}

signed int fifo_alloc(struct fifo *fifo, unsigned int size)
{
/*
	 * round down to the next power of 2, since our 'let the indices
	 * wrap' technique works only in this case.
	 */
	if (!is_power_of_2(size))
		return -1;

	fifo->in = 1;
	fifo->out = 1;

	if (size < 2){
		fifo->data = NULL;
		fifo->mask = 0;
		return -1;
	}
	fifo->data = malloc(size);

	if (!fifo->data){
		fifo->mask = 0;
		return -1;
	}
	fifo->mask = size - 1;

  return 0;
}

void fifo_free(struct fifo *fifo)
{
	free(fifo->data);
	fifo->in = 0;
	fifo->out = 0;
	fifo->data = NULL;
	fifo->mask = 0;
}

int fifo_init(struct fifo *fifo, unsigned char *buffer, unsigned int size)
{
	if (!is_power_of_2(size))
		return -1;

	fifo->in = 0;
	fifo->out = 0;
	fifo->data = buffer;

	if (size < 2) {
		fifo->mask = 0;
		return -1;
	}
	fifo->mask = size - 1;

	return 0;
}

static void fifo_copy_in(struct fifo *fifo, unsigned char *src, unsigned int len, unsigned int off)
{
	unsigned int size = fifo->mask + 1;
	unsigned int l;

	off &= fifo->mask;

	l = min(len, size - off);

	memcpy(fifo->data + off, src, l);
	memcpy(fifo->data, src + l, len - l);
}

unsigned int fifo_in(struct fifo *fifo, unsigned char *buf, unsigned int len)
{
	unsigned int l;

	l = fifo_unused(fifo);
	if (len > l)
		len = l;

	fifo_copy_in(fifo, buf, len, fifo->in);
	fifo->in += len;

	return len;
}

static void fifo_copy_out(struct fifo *fifo, unsigned char *dst, unsigned int len, unsigned int off)
{
	unsigned int size = fifo->mask + 1;
	unsigned int l;

	off &= fifo->mask;
	l = min(len, size - off);

	memcpy(dst, fifo->data + off, l);
	memcpy(dst + l, fifo->data, len - l);
}

unsigned int fifo_out_peek(struct fifo *fifo, unsigned char *buf, unsigned int len)
{
	unsigned int l;

	l = fifo->in - fifo->out;
	if (len > l)
		len = l;

	fifo_copy_out(fifo, buf, len, fifo->out);
	return len;
}

unsigned int fifo_out(struct fifo *fifo, unsigned char *buf, unsigned int len)
{
	len = fifo_out_peek(fifo, buf, len);
	fifo->out += len;
	return len;
}
