/*
 * $Header: /home/playground/src/atmega32/dcf77/interrupt0.h,v a5f312b79d67 2022/06/05 15:16:20 nkoch $
 */


#ifndef INTERRUPT0_H
#define INTERRUPT0_H


#include <stdbool.h>
#include <stdint.h>


extern long count_int0;


extern void (*interrupt0_callback) (void);
extern void interrupt0_init (void);


#endif
