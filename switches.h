/*
 * $Header: /home/playground/src/atmega32/dcf77/switches.h,v ae72af4433e2 2022/04/18 14:00:29 nkoch $
 */


#ifndef _SWITCHES_H
#define _SWITCHES_H


enum Parity
{
  none, even, odd
};


extern uint8_t read_switches ();
extern bool sw_no_debug ();
extern uint8_t sw_baudrate ();
extern uint8_t sw_databits ();
extern uint8_t sw_stopbits ();
extern uint8_t sw_parity ();


#endif
