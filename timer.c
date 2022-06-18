/* timer.c */


#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "timer.h"
#include "defs.h"


volatile int32_t microsecs;


/**********************
 * Mikrosekundentimer *
 **********************/

int32_t now (void)
{
  long us;

  do
  {
    us = microsecs;
  }
  while (us != microsecs);
  return us;
}


/***************
 * Zeitmessung *
 ***************/

void mark (int32_t *since)
{
  *since = now ();
}


int32_t elapsed (int32_t *since)
{
  return now () - *since;
}


int32_t elapsed_mark (int32_t *since)
{
  const int32_t n = now ();
  const int32_t dif = n - *since;
  *since = n;
  return dif;
}


bool is_elapsed (int32_t *since, const int32_t howlong)
{
  return elapsed (since) - howlong >= 0;
}


bool is_elapsed_mark (int32_t *since, int32_t howlong)
{
  const int32_t n = now ();
  const int32_t dif = n - *since;
  const bool is_elapsed = dif - howlong >= 0;
  if (is_elapsed)
  {
    *since = n;
  };
  return is_elapsed;
}


/********************
 * SLEEP/POWER DOWN *
 ********************/

static void no_background_action (void)
{
}


void (*sleep_background_action) (void) = no_background_action;


void sleep (void)
{
  cli ();
  set_sleep_mode (SLEEP_MODE_IDLE);
  sleep_enable ();
  sei ();
  sleep_cpu ();
  sleep_disable ();
  sleep_background_action ();
}


void sleep_until (const int32_t us)
{
  do
  {
    sleep ();
  }
  while ((now () - us) < 0);
}


void sleep_for (const long us)
{
  sleep_until (now () + us);
}
