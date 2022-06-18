/*
 * $Header: /home/playground/src/atmega32/dcf77/defs.h,v af772173f17c 2022/04/09 21:05:00 nkoch $
 */


#ifndef _DEFS_H
#define _DEFS_H


#define PORT_SWITCHES           PORTA
#define PIN_SWITCHES            PINA
#define DDR_SWITCHES            DDRA
#define MASK_SWITCHES           (_BV(PA0) | _BV(PA1) | _BV(PA2) | _BV(PA3) | _BV(PA4) | _BV(PA5) | _BV(PA6) | _BV(PA7))

#define PORT_PDN                PORTD
#define PIN_PDN                 PIND
#define DDR_PDN                 DDRD
#define MASK_PDN                (_BV(PD7))

#define LO      false
#define HI      true


#endif
