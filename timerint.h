/*
 * $Header: /home/playground/src/atmega32/dcf77/timerint.h,v dfd559393840 2022/06/05 10:28:30 nkoch $
 */


#ifndef _TIMERINT_H
#define _TIMERINT_H


extern void (*timerint0_callback) (void);
extern void timer_init (void);


#endif
