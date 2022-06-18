/*
 * $Header: /home/playground/src/atmega32/dcf77/timerint.c,v dfd559393840 2022/06/05 10:28:30 nkoch $
 */


#include <inttypes.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "common.h"
#include "defs.h"
#include "timer.h"
#include "timerint.h"


/*********************
 * Timer-0-Interrupt *
 *********************/


static void dummy (void)
{
}


void (*timerint0_callback) (void) = dummy;


ISR (TIMER0_COMP_vect)
{
  microsecs += TIMER0USECS;
  timerint0_callback ();
}


void timer_init (void)
{
#if TIMER0PRESCALE != 1024
#error
#endif
  TCCR0  = _BV(WGM01) | _BV(CS02) | _BV(CS00);
  OCR0   = TIMER0CMPVALUE;
  TCNT0  = 0;

#if TIMER1PRESCALE != 1024
#error
#endif
  TCCR1A = 0;
  TCCR1B = _BV(CS12) | _BV(CS10);
  TCNT1  = 0;

  TIMSK  = _BV(OCIE0);
}
