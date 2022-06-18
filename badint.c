/*
 * $Header: /home/playground/src/atmega32/dcf77/badint.c,v a5f312b79d67 2022/06/05 15:16:20 nkoch $
 */


#include <avr/interrupt.h>
#include "badint.h"


#define BAD_ISR(v, c)           \
ISR (v##_vect)                  \
{                               \
  ++badcount_##c;               \
}


/*******************
 * Fehlerinterrupt *
 *******************/

long badcount;


ISR (BADISR_vect)
{
  ++badcount;
}


/***************************
 * undefinierte Interrupts *
 ***************************/


long badcount_rxc;
long badcount_udre;
long badcount_txc;
long badcount_timer2_comp;
long badcount_timer2_ovf;
long badcount_timer1_capt;
long badcount_timer1_compa;
long badcount_timer1_compb;
long badcount_timer1_ovf;
long badcount_timer0_ovf;
long badcount_int1;
long badcount_int2;


BAD_ISR(USART_RXC, rxc)
BAD_ISR(USART_UDRE, udre)
BAD_ISR(USART_TXC, txc)
BAD_ISR(TIMER2_COMP, timer2_comp)
BAD_ISR(TIMER2_OVF, timer2_ovf)
BAD_ISR(TIMER1_CAPT, timer1_capt)
BAD_ISR(TIMER1_COMPA, timer1_compa)
BAD_ISR(TIMER1_COMPB, timer1_compb)
BAD_ISR(TIMER1_OVF, timer1_ovf)
BAD_ISR(TIMER0_OVF, timer0_ovf)
BAD_ISR(INT1, int1)
BAD_ISR(INT2, int2)
