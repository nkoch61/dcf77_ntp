/*
 * $Header: /home/playground/src/atmega32/dcf77/uart.c,v 833ba75e988e 2022/06/16 17:41:01 nkoch $
 */


#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>

#include "common.h"
#include "switches.h"
#define CBUF_ID         u_
#define CBUF_LEN        UART_CBUF_LEN
#define CBUF_TYPE       uint8_t
#include "cbuf.h"
#include "uart.h"


static struct u_CBuf uart_rxd;


static void dummy_sleep (void)
{
}


static void dummy_event (uint8_t c)
{
}


void (*uart_sleep) (void) = dummy_sleep;
void (*uart_inevent) (uint8_t c) = dummy_event;


static void uart_receive ()
{
  const uint8_t status = UCSRA;

  if (status & _BV(RXC))
  {
    uint8_t c = UDR;

    if (status & (_BV(FE) | _BV(DOR) | _BV(PE)))
    {
      c = '~';
    };

    if (!u_full (&uart_rxd))
    {
      u_put (&uart_rxd, c);
    }
    else
    {
      u_set_overrun (&uart_rxd);
    };
    uart_inevent (c);
  }
}


static inline bool uart_transmit (uint8_t c)
{
  if (UCSRA & _BV(UDRE))
  {
    UDR = c;
    return true;
  };
  return false;
}


int uart_getc_nowait (void)
{
  uart_receive ();
  if (!u_empty (&uart_rxd))
  {
    return u_get (&uart_rxd);
  };
  uart_sleep ();
  return -1;
}


uint8_t uart_getc (void)
{
  do
  {
    uart_receive ();
    uart_sleep ();
  }
  while (u_empty (&uart_rxd));
  return u_get (&uart_rxd);
}


bool uart_putc_nowait (uint8_t c)
{
  uart_receive ();
  return uart_transmit (c);
}


void uart_putc (uint8_t c)
{
  do
  {
    uart_receive ();
  }
  while (!uart_transmit (c));
}


int uart_in_peek (void)
{
  uart_receive ();
  return u_peek (&uart_rxd);
}


bool uart_in_empty ()
{
  uart_receive ();
  return u_empty (&uart_rxd);
}


void uart_drain ()
{
}


void uart_flush ()
{
  while (uart_getc_nowait () != -1)
  {
  }
}


static const uint8_t baudrates[][2] =
{
#define BAUD    9600
#include <util/setbaud.h>
#if USE2X
#error
#endif
  { UBRRH_VALUE, UBRRL_VALUE },
#undef BAUD

#define BAUD    4800
#include <util/setbaud.h>
#if USE2X
#error
#endif
  { UBRRH_VALUE, UBRRL_VALUE },
#undef BAUD

#define BAUD    2400
#include <util/setbaud.h>
#if USE2X
#error
#endif
  { UBRRH_VALUE, UBRRL_VALUE },
#undef BAUD

#define BAUD    1200
#include <util/setbaud.h>
#if USE2X
#error
#endif
  { UBRRH_VALUE, UBRRL_VALUE },
#undef BAUD

#define BAUD    600
#include <util/setbaud.h>
#if USE2X
#error
#endif
  { UBRRH_VALUE, UBRRL_VALUE },
#undef BAUD

#define BAUD    300
#include <util/setbaud.h>
#if USE2X
#error
#endif
  { UBRRH_VALUE, UBRRL_VALUE },
#undef BAUD

#define BAUD    300
#include <util/setbaud.h>
#if USE2X
#error
#endif
  { UBRRH_VALUE, UBRRL_VALUE },
#undef BAUD

#define BAUD    300
#include <util/setbaud.h>
#if USE2X
#error
#endif
  { UBRRH_VALUE, UBRRL_VALUE },
#undef BAUD
};


void uart_init (void)
{
  u_init (&uart_rxd);

  const uint8_t idx = sw_baudrate () % LENGTH(baudrates);
  UBRRL = baudrates[idx][1];
  UBRRH = baudrates[idx][0];
  UCSRA &= ~_BV(U2X);

  // UART Receiver und Transmitter anschalten
  UCSRB = _BV(RXEN) | _BV(TXEN);

  uint8_t ucsrc = _BV(URSEL);
  switch (sw_stopbits ())
  {
    default:
      break;

    case 2:
      ucsrc |= _BV(USBS);
      break;
  };
  switch (sw_databits ())
  {
    default:
      ucsrc |= _BV(UCSZ1) | _BV(UCSZ0);
      break;

    case 7:
      ucsrc |= _BV(UCSZ1);
      break;
  };
  switch (sw_parity ())
  {
    default:
      break;

    case even:
      ucsrc |= _BV(UPM1);
      break;

    case odd:
      ucsrc |= _BV(UPM0) | _BV(UPM1);
      break;
  };
  UCSRC = ucsrc;

  // Flush Receive-Buffer
  do
  {
    UDR;
  }
  while (UCSRA & _BV(RXC));
}


void uart_puts (const char *s)
{
  while (*s)
  {
    uart_putc (*s++);
  }
}


void uart_puts_P (const __flash char *s)
{
  char c;

  while ((c = *s++))
  {
    uart_putc (c);
  }
}


void uart_bs (void)
{
  uart_puts_P (PSTR ("\b \b"));
}


void uart_crlf (void)
{
  uart_puts_P (PSTR ("\r\n"));
}


void uart_putsln (const char *s)
{
  uart_puts (s);
  uart_crlf ();
}


void uart_putsln_P (const __flash char *s)
{
  uart_puts_P (s);
  uart_crlf ();
}


void uart_vsnprintf_P (const __flash char *fmt, va_list ap)
{
  char temp[128];
  vsnprintf_P (temp, sizeof temp, fmt, ap);
  uart_puts (temp);
}


void uart_printf_P (const __flash char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  uart_vsnprintf_P (fmt, ap);
  va_end (ap);
}
