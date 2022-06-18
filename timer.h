/*
 * $Header: /home/playground/src/atmega32/dcf77/timer.h,v 91f4daf9be75 2022/06/03 20:24:18 nkoch $
 */


#ifndef _TIMER_H
#define _TIMER_H


#include <stdbool.h>


extern volatile int32_t microsecs;

extern long now (void);
extern void mark (long *since);
extern long elapsed (long *since);
extern long elapsed_mark (long *since);
extern bool is_elapsed (int32_t *since, int32_t howlong);
extern bool is_elapsed_mark (int32_t *since, int32_t howlong);
extern void (*sleep_background_action) (void);
extern void sleep (void);
extern void sleep_until (const int32_t usecs);
extern void sleep_for (const int32_t usecs);


#endif
