/*
 * $Header: /home/playground/src/atmega32/dcf77/switches.c,v ae72af4433e2 2022/04/18 14:00:29 nkoch $
 */


#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <avr/io.h>

#include "common.h"
#include "defs.h"
#include "switches.h"


uint8_t read_switches ()
{
  return (PIN_SWITCHES & MASK_SWITCHES) ^ MASK_SWITCHES;
}


bool sw_no_debug ()
{
  return read_switches () & 0b10000000;
}


uint8_t sw_baudrate ()
{
  return read_switches () & 0b00000111;
}


uint8_t sw_stopbits ()
{
  switch (read_switches () & 0b00001000)
  {
    default:
    case 0b00000000:
      return 1;

    case 0b00001000:
      return 2;
  }
}


uint8_t sw_databits ()
{
  switch (read_switches () & 0b00010000)
  {
    default:
    case 0b00000000:
      return 8;

    case 0b00010000:
      return 7;
  }
}


uint8_t sw_parity ()
{
  switch (read_switches () & 0b01100000)
  {
    default:
    case 0b00000000:
    case 0b01100000:
      return none;

    case 0b00100000:
      return even;

    case 0b01000000:
      return odd;
  }
}
