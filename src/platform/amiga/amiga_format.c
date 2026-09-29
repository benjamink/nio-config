#include "amiga_format.h"
#include "config_nio.h"

#include <stdio.h>
#include <string.h>

void amiga_format_size(char *out, uint32_t size, uint8_t size_format)
{
  static const char *const suffix[] = { "", "Kb", "Mb", "Gb" };
  char plain[11];
  unsigned unit;
  unsigned long divisor;
  unsigned long rem;
  unsigned whole;
  unsigned tenths;

  if (size_format != CONFIG_NIO_PREF_SIZE_COMPACT) {
    uint8_t len, src, dst, digits;

    sprintf(plain, "%lu", (unsigned long) size);
    len = (uint8_t) strlen(plain);
    dst = (uint8_t) (len + (len - 1) / 3);
    out[dst] = 0;
    src = len;
    digits = 0;
    while (src > 0) {
      if (digits == 3) {
        out[--dst] = ',';
        digits = 0;
      }
      out[--dst] = plain[--src];
      digits++;
    }
    return;
  }

  /* Same rounding as the MS-DOS UI's compact size column. */
  unit = 0;
  divisor = 1UL;
  while (size / divisor >= 1000UL && unit < 3) {
    divisor *= 1024UL;
    unit++;
  }
  if (unit == 0) {
    sprintf(out, "%lu", (unsigned long) size);
    return;
  }
  whole = (unsigned) (size / divisor);
  rem = size % divisor;
  while (rem > 429496729UL && divisor > 1UL) {
    rem = (rem + 5UL) / 10UL;
    divisor = (divisor + 5UL) / 10UL;
  }
  tenths = (unsigned) ((rem * 10UL + (divisor / 2UL)) / divisor);
  if (tenths >= 10) {
    whole++;
    tenths = 0;
  }
  if (whole < 10 && tenths != 0)
    sprintf(out, "%u.%u%s", whole, tenths, suffix[unit]);
  else
    sprintf(out, "%u%s", whole, suffix[unit]);
}

/* Days since 1970-01-01 to a proleptic Gregorian date (UTC).  Avoids
 * localtime(), whose time-zone handling differs between the Amiga CRTs. */
static void civil_from_days(uint32_t days, unsigned *y, unsigned *m,
                            unsigned *d)
{
  unsigned long z, era, doe, yoe, doy, mp;

  z = (unsigned long) days + 719468UL;
  era = z / 146097UL;
  doe = z - era * 146097UL;
  yoe = (doe - doe / 1460UL + doe / 36524UL - doe / 146096UL) / 365UL;
  doy = doe - (365UL * yoe + yoe / 4UL - yoe / 100UL);
  mp = (5UL * doy + 2UL) / 153UL;
  *d = (unsigned) (doy - (153UL * mp + 2UL) / 5UL + 1UL);
  *m = (unsigned) (mp < 10UL ? mp + 3UL : mp - 9UL);
  *y = (unsigned) (yoe + era * 400UL + (*m <= 2 ? 1UL : 0UL));
}

int amiga_format_date(char *out, uint32_t mtime, uint8_t date_format)
{
  unsigned y, m, d;

  if (mtime == 0) {
    strcpy(out, "?\?-?\?-?\?");
    return 0;
  }
  civil_from_days(mtime / 86400UL, &y, &m, &d);
  if (date_format == CONFIG_NIO_PREF_DATE_YDM)
    sprintf(out, "%02u-%02u-%02u", y % 100, d, m);
  else
    sprintf(out, "%02u-%02u-%02u", y % 100, m, d);
  return 1;
}

static void copy_bounded(char *out, uint16_t cap, const char *s, uint16_t n)
{
  if (n >= cap)
    n = (uint16_t) (cap - 1);
  memcpy(out, s, n);
  out[n] = 0;
}

void amiga_clip_head(char *out, uint16_t cap, const char *s, uint8_t max_chars)
{
  uint16_t len;

  if (!out || cap == 0)
    return;
  if (!s)
    s = "";
  len = (uint16_t) strlen(s);
  if (len <= max_chars || max_chars < 4) {
    copy_bounded(out, cap, s, len < max_chars ? len : max_chars);
    return;
  }
  copy_bounded(out, cap, s, (uint16_t) (max_chars - 3));
  if ((uint16_t) (strlen(out) + 3) < cap)
    strcat(out, "...");
}

void amiga_clip_tail(char *out, uint16_t cap, const char *s, uint8_t max_chars)
{
  uint16_t len;

  if (!out || cap == 0)
    return;
  if (!s)
    s = "";
  len = (uint16_t) strlen(s);
  if (len <= max_chars || max_chars < 4) {
    copy_bounded(out, cap, s + (len > max_chars ? len - max_chars : 0),
                 len < max_chars ? len : max_chars);
    return;
  }
  if (cap < 4) {
    out[0] = 0;
    return;
  }
  strcpy(out, "...");
  copy_bounded(out + 3, (uint16_t) (cap - 3), s + len - (max_chars - 3),
               (uint16_t) (max_chars - 3));
}

static int is_shell_trailer(const char *line, const char *stop)
{
  static const char marker[] = " failed returncode ";
  size_t n = sizeof(marker) - 1;
  const char *p;

  for (p = line; p + n <= stop; p++) {
    if (memcmp(p, marker, n) == 0)
      return 1;
  }
  return 0;
}

/* Copies the last non-blank line, preferring any line over the Shell's
 * "<cmd> failed returncode N" trailer, which only repeats the return code. */
void amiga_last_line(char *out, uint16_t cap, const char *text)
{
  const char *start = NULL;
  const char *end = NULL;
  const char *trailer = NULL;
  const char *trailer_end = NULL;
  const char *p;

  if (!out || cap == 0)
    return;
  out[0] = 0;
  if (!text)
    return;
  for (p = text; *p;) {
    const char *line = p;
    const char *stop;
    const char *q;

    while (*p && *p != '\n')
      p++;
    stop = p;
    while (stop > line && (stop[-1] == '\r' || stop[-1] == ' ' ||
                           stop[-1] == '\t'))
      stop--;
    for (q = line; q < stop && (*q == ' ' || *q == '\t'); q++)
      ;
    if (q < stop) {
      if (is_shell_trailer(line, stop)) {
        trailer = line;
        trailer_end = stop;
      } else {
        start = line;
        end = stop;
      }
    }
    if (*p == '\n')
      p++;
  }
  if (!start) {
    start = trailer;
    end = trailer_end;
  }
  if (start)
    copy_bounded(out, cap, start, (uint16_t) (end - start));
}
