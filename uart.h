/*
 * $Header: /home/playground/src/atmega32/dcf77/uart.h,v c4dfb6d4e630 2022/04/09 16:48:31 nkoch $
 */


#ifndef _UART_H
#define _UART_H


#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>


extern void (*uart_sleep) (void);
extern void (*uart_inevent) (uint8_t c);
extern int uart_getc_nowait (void);
extern uint8_t uart_getc (void);
extern bool uart_putc_nowait (uint8_t c);
extern void uart_putc (uint8_t c);
extern void uart_drain ();
extern void uart_flush ();
extern int uart_in_peek (void);
extern bool uart_in_empty ();
extern uint16_t uart_in_used ();
extern uint16_t uart_out_avail ();
extern void uart_init (void);

extern void uart_puts (const char *s);
extern void uart_puts_P (const __flash char *s);
extern void uart_bs (void);
extern void uart_crlf (void);
extern void uart_putsln (const char *s);
extern void uart_putsln_P (const __flash char *s);
extern void uart_vsnprintf_P (const __flash char *fmt, va_list ap);
extern void uart_printf_P (const __flash char *fmt, ...);

#endif
