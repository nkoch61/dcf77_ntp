/*
 * $Header: /home/playground/src/atmega32/dcf77/main.c,v dfa21dbd23b0 2022/06/17 18:27:12 nkoch $
 */


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <util/delay.h>

#include "common.h"
#include "switches.h"
#include "cmdint.h"
#include "uart.h"
#include "timer.h"
#include "timerint.h"
#include "interrupt0.h"
#include "badint.h"

#include "defs.h"


#define FORMAT_PZF5X    1
#define FORMAT_HOPF6021 2

#ifndef FORMAT
#error
#endif


static const __flash char program_version[] = "1.1.3 " __DATE__ " " __TIME__;


enum ProtocolState
{
  NOT_SYNCED = 0,
  START_OF_MINUTE,
  IGNORE_1,
  IGNORE_2,
  IGNORE_3,
  IGNORE_4,
  IGNORE_5,
  IGNORE_6,
  IGNORE_7,
  IGNORE_8,
  IGNORE_9,
  IGNORE_10,
  IGNORE_11,
  IGNORE_12,
  IGNORE_13,
  IGNORE_14,
  IGNORE_15,
  NEW_TZ,
  CEST,
  CET,
  LEAP,
  START_TIME,
  MIN_0,
  MIN_1,
  MIN_2,
  MIN_3,
  MIN_4,
  MIN_5,
  MIN_6,
  PARITY_MIN,
  HR_0,
  HR_1,
  HR_2,
  HR_3,
  HR_4,
  HR_5,
  PARITY_HR,
  DAY_0,
  DAY_1,
  DAY_2,
  DAY_3,
  DAY_4,
  DAY_5,
  WDAY_0,
  WDAY_1,
  WDAY_2,
  MON_0,
  MON_1,
  MON_2,
  MON_3,
  MON_4,
  YR_0,
  YR_1,
  YR_2,
  YR_3,
  YR_4,
  YR_5,
  YR_6,
  YR_7,
  PARITY_DATE,
  END_OF_MINUTE,
};

enum ERROR
{
  NO_ERROR,
  E_STATE,
  E_START_OF_MINUTE,
  E_END_OF_MINUTE,
  E_NOT_END_OF_MINUTE,
  E_CET_CEST,
  E_START_TIME,
  E_PARITY_MIN,
  E_PARITY_HR,
  E_PARITY_DATE,
  E_BIT_STATE,
};



static uint8_t sec, sec_max = 59;
static volatile bool new_time_info, inc_min;
static bool valid_time_info, invalid_time_info;
static bool valid_time_info_once, quartz_time;
static bool pll_synced;
static uint8_t ei_state, ti_state, last_ti_state, bit_state, bit_count[2];
static uint16_t last_tcnt0, last_tcnt1;
static uint8_t state, err_state;
static int err_line;
static uint8_t last_err;
static uint16_t err_count;
static uint8_t switches_at_start;

struct TimeInfo
{
  char min[2], hr[2], day[2], wday[1], mon[2], yr[2];
  bool tz_change, cet, cest, leap;
};

static struct TimeInfo time_info[2], cached_time_info, *wi, *ri;

#define STX     "\x02"
#define ETX     "\x03"
#define CR      "\r"
#define LF      "\n"

#if FORMAT == FORMAT_PZF5X
/* Uni Erlangen time string for PZF5xx receivers (9600E72, NTP mode 2): */
static char time_string[] = STX "dd.mm.yy; w; hh:mm:ss; tuvxyza" ETX;
#endif
#if FORMAT == FORMAT_HOPF6021
/* Hopf 6021 receivers (9600N81, NTP mode 12): */
static char time_string[] = STX "ABhhmmssddmmyy" LF CR ETX;
#endif


static const __flash struct
{
  const __flash char *name;
}
errors[] =
{
  [NO_ERROR             ]       FSTR("NO ERROR"),
  [E_STATE              ]       FSTR("UNDEFINED STATE"),
  [E_START_OF_MINUTE    ]       FSTR("START OF MINUTE"),
  [E_END_OF_MINUTE      ]       FSTR("END OF MINUTE"),
  [E_NOT_END_OF_MINUTE  ]       FSTR("NOT END OF MINUTE"),
  [E_CET_CEST           ]       FSTR("CET/CEST"),
  [E_START_TIME         ]       FSTR("START OF TIME"),
  [E_PARITY_MIN         ]       FSTR("PARITY MINUTE"),
  [E_PARITY_HR          ]       FSTR("PARITY HOUR"),
  [E_PARITY_DATE        ]       FSTR("PARITY DATE"),
  [E_BIT_STATE          ]       FSTR("BIT STATE"),
};


/**********************************
 * Hintergrundaktion nach sleep() *
 **********************************/


static void reset_cpu (void)
{
  wdt_enable (WDTO_15MS);
  cli ();
}


static void do_inc_min (void)
{
  if (cached_time_info.min[1] < '9')
  {
    ++cached_time_info.min[1];
    return;
  };

  cached_time_info.min[1] = '0';
  if (cached_time_info.min[0] < '5')
  {
    ++cached_time_info.min[0];
    return;
  };

  cached_time_info.min[0] = '0';
  if ((cached_time_info.hr[0] < '2' && cached_time_info.hr[1] < '9')
      ||
      cached_time_info.hr[1] < '3')
  {
    ++cached_time_info.hr[1];
    return;
  };

  cached_time_info.hr[1] = '0';
  if (cached_time_info.hr[0] < '2')
  {
    ++cached_time_info.hr[0];
    return;
  };

  cached_time_info.hr[0] = '0';
}


static void background (void)
{
  static uint8_t recursive = 0;

  if (++recursive > 1)
  {
    --recursive;
    return;
  };

  static uint8_t prev_sec = 69;

  if (new_time_info)
  {
    cached_time_info = *ri;
    new_time_info = false;
    valid_time_info_once = true;
    quartz_time = false;
  }
  else if (inc_min && valid_time_info_once)
  {
    do_inc_min ();
    quartz_time = true;
    inc_min = false;
  };

  if (valid_time_info_once && prev_sec != sec)
  {
    prev_sec = sec;
#if FORMAT == FORMAT_PZF5X
    snprintf_P (time_string,
                sizeof time_string,
                PSTR(STX "%02.2s.%02.2s.%02.2s; %1.1s; %02.2s:%02.2s:%02.2u; %c%c%c%c%c%c%c" ETX),
                /*                                                            t u v x y z a */
                cached_time_info.day,
                cached_time_info.mon,
                cached_time_info.yr,
                cached_time_info.wday,
                cached_time_info.hr,
                cached_time_info.min,
                prev_sec,
                ' ',                                            /* t: Lokalzeit */
                ' ',                                            /* u: wenigstens einmal synchronisiert */
                quartz_time                ? '*' : ' ',         /* v: Quarz j/n */
                cached_time_info.cest      ? 'S' : ' ',         /* x: Sommer-/Winterzeit */
                cached_time_info.tz_change ? '!' : ' ',         /* y: Wechsel Sommer-/Winterzeit anstehend */
                cached_time_info.leap      ? 'A' : ' ',         /* z: Schaltsekunde anstehend */
                ' ');                                           /* a: */
#endif
#if FORMAT == FORMAT_HOPF6021
    const uint8_t status =
      (quartz_time                ? 0b01000000 : 0b11000000) |
      (cached_time_info.cest      ? 0b00100000 : 0b00000000) |
      (cached_time_info.tz_change ? 0b00010000 : 0b00000000) |
      (cached_time_info.wday[0] & 0b00000111);
    snprintf_P (time_string,
                sizeof time_string,
                PSTR(STX "%02.2X%02.2s%02.2s%02.2u%02.2s%02.2s%02.2s" LF CR ETX),
                status,
                cached_time_info.hr,
                cached_time_info.min,
                prev_sec,
                cached_time_info.day,
                cached_time_info.mon,
                cached_time_info.yr);
#endif
    if (sw_no_debug ())
    {
      /* Zeitinformation senden */
      uart_puts (time_string);
    }
  }

  if (read_switches () != switches_at_start)
  {
    reset_cpu ();
  };

  --recursive;
}


/**************************
 * Zustand initialisieren *
 *************************/

static void set_error (uint8_t err, int line)
{
  if (last_err == NO_ERROR)
  {
    err_line = line;
    err_state = state;
    last_err = err;
  };
  err_count += err != NO_ERROR;
}


static void not_synced (uint8_t err, int line)
{
  set_error (err, line);
  state = NOT_SYNCED;
}


/************************
 * neue Zeitinformation *
 ************************/

static void update_time_info (void)
{
  wi = wi == &time_info[1] ? &time_info[0] : &time_info[1],     /* SEQUENCE POINT */
  ri = wi == &time_info[1] ? &time_info[0] : &time_info[1];
  new_time_info = true;
  inc_min = false;
}


/*************************
 * Protokollverarbeitung *
 *************************/

static void protocol ()
{
  static uint8_t parity;
  uint8_t bit = 0;

  switch (bit_state)
  {
    case 0b00:  /* "1" */
      bit = 1;
      break;

    case 0b01:  /* "0" */
      break;

    case 0b11:
      if (state != END_OF_MINUTE)
      {
        state = END_OF_MINUTE;
        set_error (E_NOT_END_OF_MINUTE, __LINE__);
      };
      break;

    default:
      not_synced (E_BIT_STATE, __LINE__);
      break;
  };

  switch (state)
  {
    case NOT_SYNCED:
      valid_time_info = false;
      return;

    case START_OF_MINUTE:
      if (bit != 0)
      {
        set_error (E_START_OF_MINUTE, __LINE__);
        invalid_time_info = true;
      }
      else
      {
        sec = 0;
        invalid_time_info = false;
      };
      break;

    case IGNORE_1:
    case IGNORE_2:
    case IGNORE_3:
    case IGNORE_4:
    case IGNORE_5:
    case IGNORE_6:
    case IGNORE_7:
    case IGNORE_8:
    case IGNORE_9:
    case IGNORE_10:
    case IGNORE_11:
    case IGNORE_12:
    case IGNORE_13:
    case IGNORE_14:
    case IGNORE_15:
      break;

    case NEW_TZ:
      wi->tz_change = bit == 1;
      break;

    case CEST:
      wi->cest = bit == 1;
      break;

    case CET:
      wi->cet = bit == 1;
      if (wi->cet == wi->cest)
      {
        invalid_time_info = true;
        set_error (E_CET_CEST, __LINE__);
      };
      break;

    case LEAP:
      wi->leap = bit == 1;
      break;

    case START_TIME:
      parity = 0;
      wi->min[0] = '0';
      wi->min[1] = '0';
      if (bit != 1)
      {
        invalid_time_info = true;
        set_error (E_START_TIME, __LINE__);
      };
      break;

    case MIN_0:
      parity ^= bit;
      wi->min[1] |= (bit << 0);
      break;

    case MIN_1:
      parity ^= bit;
      wi->min[1] |= (bit << 1);
      break;

    case MIN_2:
      parity ^= bit;
      wi->min[1] |= (bit << 2);
      break;

    case MIN_3:
      parity ^= bit;
      wi->min[1] |= (bit << 3);
      break;

    case MIN_4:
      parity ^= bit;
      wi->min[0] |= (bit << 0);
      break;

    case MIN_5:
      parity ^= bit;
      wi->min[0] |= (bit << 1);
      break;

    case MIN_6:
      parity ^= bit;
      wi->min[0] |= (bit << 2);
      break;

    case PARITY_MIN:
      wi->hr[0] = '0';
      wi->hr[1] = '0';
      if (bit != parity)
      {
        invalid_time_info = true;
        set_error (E_PARITY_MIN, __LINE__);
      };
      parity = 0;
      break;

    case HR_0:
      parity ^= bit;
      wi->hr[1] |= (bit << 0);
      break;

    case HR_1:
      parity ^= bit;
      wi->hr[1] |= (bit << 1);
      break;

    case HR_2:
      parity ^= bit;
      wi->hr[1] |= (bit << 2);
      break;

    case HR_3:
      parity ^= bit;
      wi->hr[1] |= (bit << 3);
      break;

    case HR_4:
      parity ^= bit;
      wi->hr[0] |= (bit << 0);
      break;

    case HR_5:
      parity ^= bit;
      wi->hr[0] |= (bit << 1);
      break;

    case PARITY_HR:
      wi->day[0] = '0';
      wi->day[1] = '0';
      if (bit != parity)
      {
        invalid_time_info = true;
        set_error (E_PARITY_HR, __LINE__);
      };
      parity = 0;
      break;

    case DAY_0:
      parity ^= bit;
      wi->day[1] |= (bit << 0);
      break;

    case DAY_1:
      parity ^= bit;
      wi->day[1] |= (bit << 1);
      break;

    case DAY_2:
      parity ^= bit;
      wi->day[1] |= (bit << 2);
      break;

    case DAY_3:
      parity ^= bit;
      wi->day[1] |= (bit << 3);
      break;

    case DAY_4:
      parity ^= bit;
      wi->day[0] |= (bit << 0);
      break;

    case DAY_5:
      parity ^= bit;
      wi->day[0] |= (bit << 1);
      wi->wday[0] = '0';
      break;

    case WDAY_0:
      parity ^= bit;
      wi->wday[0] |= (bit << 0);
      break;

    case WDAY_1:
      parity ^= bit;
      wi->wday[0] |= (bit << 1);
      break;

    case WDAY_2:
      parity ^= bit;
      wi->wday[0] |= (bit << 2);
      wi->mon[0] = '0';
      wi->mon[1] = '0';
      break;

    case MON_0:
      parity ^= bit;
      wi->mon[1] |= (bit << 0);
      break;

    case MON_1:
      parity ^= bit;
      wi->mon[1] |= (bit << 1);
      break;

    case MON_2:
      parity ^= bit;
      wi->mon[1] |= (bit << 2);
      break;

    case MON_3:
      parity ^= bit;
      wi->mon[1] |= (bit << 3);
      break;

    case MON_4:
      parity ^= bit;
      wi->mon[0] |= (bit << 0);
      wi->yr[0] = '0';
      wi->yr[1] = '0';
      break;

    case YR_0:
      parity ^= bit;
      wi->yr[1] |= (bit << 0);
      break;

    case YR_1:
      parity ^= bit;
      wi->yr[1] |= (bit << 1);
      break;

    case YR_2:
      parity ^= bit;
      wi->yr[1] |= (bit << 2);
      break;

    case YR_3:
      parity ^= bit;
      wi->yr[1] |= (bit << 3);
      break;

    case YR_4:
      parity ^= bit;
      wi->yr[0] |= (bit << 0);
      break;

    case YR_5:
      parity ^= bit;
      wi->yr[0] |= (bit << 1);
      break;

    case YR_6:
      parity ^= bit;
      wi->yr[0] |= (bit << 2);
      break;

    case YR_7:
      parity ^= bit;
      wi->yr[0] |= (bit << 3);
      break;

    case PARITY_DATE:
      if (bit != parity)
      {
        invalid_time_info = true;
        set_error (E_PARITY_DATE, __LINE__);
      };
      valid_time_info = !invalid_time_info;
      break;

    case END_OF_MINUTE:
      switch (bit_state)
      {
        case 0b01:      // Schaltsekunde
          sec_max = 60;
          return;

        case 0b11:
          state = START_OF_MINUTE;
          return;

        default:
          not_synced (E_END_OF_MINUTE, __LINE__);
          return;
      };

    default:
      not_synced (E_STATE, __LINE__);
      return;
  };

  ++state;
}


/******************
 * Timerinterrupt *
 ******************/

static void ti_S0 ();
static void ti_S1 ();
static void ti_S2 ();
static void ti_S3 ();
static void ti_S4 ();
static void ti_S5 ();
static void ti_S6 ();
static void ti_S7 ();
static void ti_S8 ();
static void ti_S9 ();
static void ti_S10 ();
static void ti_S11 ();
static void ti_S12 ();


#define ti_STATE(n)                     \
  do                                    \
  {                                     \
    ti_state = n;                       \
    timerint0_callback = XCAT(ti_S,n);  \
  }                                     \
  while (false);


static void ti_S0 ()
{
  if (pll_synced)
  {
    ti_STATE(1);
    if (++sec > sec_max)
    {
      sec = 0;
      sec_max = 59;
    }
  }
}

#define SIGNAL_STATE()  (!!(PIND & _BV(PD2)))


static void ti_S1 ()
{
  bit_count[0] = SIGNAL_STATE();
  ti_STATE(2);
}


static void ti_S2 ()
{
  bit_count[0] += SIGNAL_STATE();
  ti_STATE(3);
}


static void ti_S3 ()
{
  bit_count[0] += SIGNAL_STATE();
  ti_STATE(4);
}


static void ti_S4 ()
{
  bit_state = bit_count[0] <= 1 ? 0b00 : 0b10;
  ti_STATE(5);
}


static void ti_S5 ()
{
  ti_STATE(6);
}


static void ti_S6 ()
{
  ti_STATE(7);
}


static void ti_S7 ()
{
  ti_STATE(8);
}


static void ti_S8 ()
{
  bit_count[1] = SIGNAL_STATE();
  ti_STATE(9);
}


static void ti_S9 ()
{
  bit_count[1] += SIGNAL_STATE();
  ti_STATE(10);
}


static void ti_S10 ()
{
  bit_count[1] += SIGNAL_STATE();
  ti_STATE(11);
}


static void ti_S11 ()
{
  bit_state |= bit_count[1] <= 1 ? 0b00 : 0b01;
  protocol ();
  if (sec == 0)
  {
    if (valid_time_info)
    {
      update_time_info ();
    }
    else
    {
      inc_min = true;
    };
    valid_time_info = false;
  };
  ti_STATE(12);
}


static void ti_S12 ()
{
#define TI_LASTSTATE()  (1000000/TIMER0USECS-1)
  if (ti_state >= TI_LASTSTATE())
  {
    ti_STATE(0);
  }
  else
  {
    ++ti_state;
  }
}


/*******************************
 * Flanken-Interrupt Bit Start *
 *******************************/

static void ei_S0 (void);
static void ei_S1 (void);
static void ei_S2 (void);
static void ei_S3 (void);


#define ei_STATE(n)                     \
  do                                    \
  {                                     \
    ei_state = n;                       \
    interrupt0_callback = XCAT(ei_S,n); \
  }                                     \
  while (false);


static inline bool valid_edge (void)
{
  (last_tcnt1 = TCNT1), TCNT1 = 0;

  const bool ok_1s = 1*15*(TIMER1VALUE_1S/16) <= last_tcnt1 && last_tcnt1 <= 1*17*(TIMER1VALUE_1S/16);
  const bool ok_2s = 2*15*(TIMER1VALUE_1S/16) <= last_tcnt1 && last_tcnt1 <= 2*17*(TIMER1VALUE_1S/16);
  return ok_1s || ok_2s;
}


static void ei_S0 (void)
{
  TCNT1 = 0;
  ei_STATE(1);
}


static void ei_S1 (void)
{
  if (valid_edge ())
  {
    ei_STATE(2);
  }
  else
  {
    ei_STATE(0);
  }
}


static void ei_S2 (void)
{
  if (valid_edge ())
  {
    ei_STATE(3);
  }
  else
  {
    ei_STATE(0);
  }
}


static void ei_S3 (void)
{
  if (valid_edge ())
  {
    (last_tcnt0 = TCNT0), (TCNT0 = TIMER0CMPVALUE/2);
    last_ti_state = ti_state;
    pll_synced = true;
    ti_STATE(0);
  }
  else
  {
    ei_STATE(0);
  }
}


/*************
 * Kommandos *
 ************/

static bool cont (int8_t argc, char **argv)
{
  const bool do_cont = argc > 1 && strcmp (argv[1], "-c") == 0;
  if (do_cont && uart_getc_nowait () == -1)
  {
    sleep_for (250);
    return true;
  }
  else
  {
    uart_crlf ();
    return false;
  }
}


static int8_t last_error (int8_t argc, char **argv)
{
  if (last_err >= NO_ERROR && last_err < LENGTH (errors))
  {
    uart_puts_P (errors[last_err].name);
    if (last_err != NO_ERROR)
    {
      uart_printf_P (PSTR(" state=%u line=%u"), err_state, err_line);
    };
    uart_crlf ();
  };
  last_err = NO_ERROR;
  return 0;
}


static int8_t error_count (int8_t argc, char **argv)
{
  const uint16_t errc = err_count;
  err_count = 0;
  uart_printf_P (PSTR("%u\r\n"), errc);
  return 0;
}


static int8_t last_sec (int8_t argc, char **argv)
{
  do
  {
    uart_printf_P (PSTR("%02.2u\r"), sec);
  }
  while (cont (argc, argv));
  return 0;
}


static int8_t last_bit (int8_t argc, char **argv)
{
  do
  {
    uart_printf_P (PSTR("%2.2u c=%1.1u/%1.1u s=%1.1u%1.1u\r"),
                   sec,
                   bit_count[0], bit_count[1],
                   !!(bit_state & 0b10), !!(bit_state & 0b01));
  }
  while (cont (argc, argv));
  return 0;
}


static int8_t last_state (int8_t argc, char **argv)
{
  do
  {
    uart_printf_P (PSTR("ei_state=%2.2u ti_state=%2.2u(%2.2u) state=%2.2u\r"), ei_state, ti_state, last_ti_state, state);
  }
  while (cont (argc, argv));
  return 0;
}


static int8_t last_timer (int8_t argc, char **argv)
{
  do
  {
    uart_printf_P (PSTR("last_tcnt0=%05.5u last_tcnt1=%05.5u\r"),
                   last_tcnt0, last_tcnt1);
  }
  while (cont (argc, argv));
  return 0;
}


static int8_t last_time_info (int8_t argc, char **argv)
{
  if (valid_time_info_once)
  {
    do
    {
      uart_printf_P (PSTR("20%02.2s-%02.2s-%02.2s [w=%1.1s, d=%1.1u] %02.2s:%02.2s:%02.2u\r"),
                     cached_time_info.yr,
                     cached_time_info.mon,
                     cached_time_info.day,
                     cached_time_info.wday,
                     cached_time_info.cest,
                     cached_time_info.hr,
                     cached_time_info.min,
                     sec);
    }
    while (cont (argc, argv));
  };
  return 0;
}


static int8_t last_time_string (int8_t argc, char **argv)
{
  if (valid_time_info_once)
  {
    do
    {
      char temp_time_string[80], *p;

      strlcpy (temp_time_string, time_string, sizeof temp_time_string);
      while ((p = strpbrk (temp_time_string, STX ETX CR LF)))
      {
        *p = '~';
      };

      uart_puts (temp_time_string);
      uart_putc ('\r');
    }
    while (cont (argc, argv));
  };
  return 0;
}


/*****************************
 * Kommandoschnittstelle (1) *
 *****************************/

static void uart_cancel (void)
{
  uart_puts_P (PSTR ("#"));
}


static void uart_prompt (void)
{
  uart_puts_P (PSTR (">"));
}


/*****************************
 * Kommandoschnittstelle (2) *
 *****************************/

static int8_t istat (int8_t argc, char **argv);
static int8_t switches (int8_t argc, char **argv);
static int8_t reset (int8_t argc, char **argv);
static int8_t help (int8_t argc, char **argv);
static int8_t version (int8_t argc, char **argv);


static const __flash struct CMD
{
  const __flash char *name;
  int8_t (*func) (int8_t argc, char **argv);
} cmds[] =
{
  { .name = FSTR("err"),         .func = last_error      },
  { .name = FSTR("errc"),        .func = error_count     },
  { .name = FSTR("sec"),         .func = last_sec        },
  { .name = FSTR("bit"),         .func = last_bit        },
  { .name = FSTR("state"),       .func = last_state      },
  { .name = FSTR("timer"),       .func = last_timer      },
  { .name = FSTR("time"),        .func = last_time_info  },
  { .name = FSTR("ts"),          .func = last_time_string},
  { .name = FSTR("istat"),       .func = istat           },
  { .name = FSTR("switches"),    .func = switches        },
  { .name = FSTR("reset"),       .func = reset           },
  { .name = FSTR("?"),           .func = help            },
  { .name = FSTR("ver"),         .func = version         },
};


/* Interruptstatistik */
static int8_t istat (int8_t argc, char **argv)
{
  uart_printf_P (PSTR("up=%luµs bad=%lu\r\n"), microsecs, badcount);
  uart_printf_P (PSTR("bad_rxc=%lu\r\n"), badcount_rxc);
  uart_printf_P (PSTR("bad_udre=%lu\r\n"), badcount_udre);
  uart_printf_P (PSTR("bad_txc=%lu\r\n"), badcount_txc);
  uart_printf_P (PSTR("bad_timer2_comp=%lu\r\n"), badcount_timer2_comp);
  uart_printf_P (PSTR("bad_timer2_ovf=%lu\r\n"), badcount_timer2_ovf);
  uart_printf_P (PSTR("bad_timer1_capt=%lu\r\n"), badcount_timer1_capt);
  uart_printf_P (PSTR("bad_timer1_compa=%lu\r\n"), badcount_timer1_compa);
  uart_printf_P (PSTR("bad_timer1_compb=%lu\r\n"), badcount_timer1_compb);
  uart_printf_P (PSTR("bad_timer1_ovf=%lu\r\n"), badcount_timer1_ovf);
  uart_printf_P (PSTR("bad_timer0_ovf=%lu\r\n"), badcount_timer0_ovf);
  uart_printf_P (PSTR("int0=%lu\r\n"), count_int0);
  uart_printf_P (PSTR("bad_int1=%lu\r\n"), badcount_int1);
  uart_printf_P (PSTR("bad_int2=%lu"), badcount_int2);
  return 0;
}


/* DIP-Switches */
static int8_t switches (int8_t argc, char **argv)
{
  uart_printf_P (PSTR("%x\r\n"), read_switches ());
  return 0;
}


/* CPU-Reset */
static int8_t reset (int8_t argc, char **argv)
{
  reset_cpu ();
  return 0;
}


static int8_t help (int8_t argc, char **argv)
{
  for (int8_t i = -1; ++i < LENGTH (cmds);)
  {
    uart_putc (' ');
    uart_puts_P (cmds[i].name);
  };
  return 0;
}


static int8_t version (int8_t argc, char **argv)
{
  uart_puts_P (program_version);
  return 0;
}


static int8_t line (void)
{
  return 0;
}


static int8_t interp (int8_t argc, char **argv)
{
  if (argc >= 1)
  {
    for (int8_t i = -1; ++i < LENGTH (cmds);)
    {
      const __flash char *name = cmds[i].name;
      if (strcmp_P (argv[0], name) == 0)
      {
        if (cmds[i].func (argc, argv) == 0)
        {
          uart_crlf ();
          return 0;
        };
        break;
      }
    }
  };
  uart_putsln_P (PSTR ("?"));
  return 0;
}


/*******************
 * Initialisierung *
 *******************/


uint8_t mcusr_mirror __attribute__ ((section (".noinit")));


void get_mcusr(void) \
  __attribute__((naked)) \
  __attribute__((section(".init3")));

void get_mcusr(void)
{
  mcusr_mirror = MCUSR;
  MCUSR = 0;
  wdt_disable();
}


static void init (void)
{
  cli ();

  /* Eingaenge DIP-Switches */
  DDR_SWITCHES  &= ~MASK_SWITCHES;
  PORT_SWITCHES |=  MASK_SWITCHES;

  /* Ausgang PDN */
  DDR_PDN  |=  MASK_PDN;
  PORT_PDN |=  MASK_PDN;

  uart_init ();
  interrupt0_init ();
  timer_init ();

  sleep_background_action = background;
  uart_sleep = sleep;

  ti_STATE(0);
  ei_STATE(0);

  ri = NULL;
  wi = &time_info[0];

  sei ();

  switches_at_start = read_switches ();
}


static void signon_message (void)
{
  if (sw_no_debug ())
  {
    return;
  };

  uart_puts_P (PSTR ("\r\n\n\nDCF77 "));
  uart_putsln_P (program_version);
}


/*************************
 * Hauptprogrammschleife *
 *************************/

int main (void)
{
  static const struct CmdIntCallback cic =
  {
    .getc_f   = uart_getc,
    .putc_f   = uart_putc,
    .bs_f     = uart_bs,
    .crlf_f   = uart_crlf,
    .cancel_f = uart_cancel,
    .prompt_f = uart_prompt,
    .interp_f = interp,
    .line_f   = line
  };

  init ();
  signon_message ();

  PORT_PDN &= ~MASK_PDN;

  while (sw_no_debug ())
  {
    sleep ();
  };

  for (;;)
  {
    cmdint (&cic);
  };
  return 0;
}
