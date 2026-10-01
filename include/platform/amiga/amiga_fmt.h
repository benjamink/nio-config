#ifndef AMIGA_FMT_H
#define AMIGA_FMT_H

#include <stdarg.h>

/* A small sprintf used by all Amiga code instead of the C library's.  The
 * WB1.3 build's libnix formats through Kickstart 1.3's RawDoFmt, which has
 * no %u and a 16-bit %d.  Supports %s %c %d %u %ld %lu %x %%, width, '-',
 * '0' and precision on %s.  Returns the length written. */
int amiga_sprintf(char *out, const char *fmt, ...);
int amiga_vsprintf(char *out, const char *fmt, va_list ap);

#endif
