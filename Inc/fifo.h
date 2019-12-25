/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _FIFO_H
#define _FIFO_H

// #include <stm32f10x.h>

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
/* Define --------------------------------------------------------------------*/
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define is_power_of_2(x)	((x) != 0 && (((x) & ((x) - 1)) == 0))

/* Private typedef -----------------------------------------------------------*/
struct fifo {
	unsigned int	in;
	unsigned int	out;
	unsigned int	mask;
	unsigned char *data;
};

/* Function prototypes -------------------------------------------------------*/
extern unsigned int fifo_used(struct fifo *fifo);
extern signed int fifo_alloc(struct fifo *fifo, unsigned int size);
extern void         fifo_free(struct fifo *fifo);
extern int          fifo_init(struct fifo *fifo, unsigned char *buffer,	unsigned int size);
extern unsigned int fifo_in(struct fifo *fifo, unsigned char *buf, unsigned int len);
extern unsigned int fifo_out(struct fifo *fifo,	unsigned char *buf, unsigned int len);

#ifdef __cplusplus
}
#endif
#endif
