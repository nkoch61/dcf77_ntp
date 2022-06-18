/*
 * $Header: /home/playground/src/atmega32/dcf77/interrupt0.c,v a5f312b79d67 2022/06/05 15:16:20 nkoch $
 */


#include <inttypes.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "common.h"
#include "interrupt0.h"


long count_int0;


static void dummy (void)
{
}


void (*interrupt0_callback) (void) = dummy;


// EXTINT0-Interrupt
ISR (INT0_vect)
{
  ++count_int0;
  interrupt0_callback ();
}


void interrupt0_init (void)
{
  GICR  &= ~_BV(INT0);
  MCUCR &= ~_BV(ISC00);
  MCUCR |=  _BV(ISC01);
  GIFR  |=  _BV(INTF0);
  GICR  |=  _BV(INT0);
}
