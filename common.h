/*
 * $Header: /home/playground/src/atmega32/dcf77/common.h,v c4dfb6d4e630 2022/04/09 16:48:31 nkoch $
 */


#ifndef _COMMON_H
#define _COMMON_H


#define CAT(a,b)                a##b
#define XCAT(a,b)               CAT (a,b)
#define STR(x)                  #x
#define XSTR(x)                 STR(x)
#define ASIZE(a)                (sizeof (a) / sizeof ((a)[0]))
#define LENGTH(a)               (ASIZE (a))
#define FIELD(t, f)             (((t*)0)->f)
#define FIELD_SIZEOF(t, f)      (sizeof(FIELD(t, f)))

#define RNDDIV(x, y)    (((x) + ((y) >> 1)) / (y))


#define FSTR(x)         (const __flash char []) {x}


#endif
