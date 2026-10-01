#include "amiga_fmt.h"

#include <string.h>

static char *put_padded(char *o, const char *s, int len, int width, int left,
                        char pad)
{
  int fill = width > len ? width - len : 0;

  if (!left)
    while (fill-- > 0)
      *o++ = pad;
  memcpy(o, s, (size_t) len);
  o += len;
  if (left)
    while (fill-- > 0)
      *o++ = ' ';
  return o;
}

int amiga_vsprintf(char *out, const char *fmt, va_list ap)
{
  char *o = out;
  char num[24];

  while (*fmt) {
    int left = 0;
    int zero = 0;
    int width = 0;
    int prec = -1;
    int is_long = 0;
    char conv;

    if (*fmt != '%') {
      *o++ = *fmt++;
      continue;
    }
    fmt++;
    for (;; fmt++) {
      if (*fmt == '-')
        left = 1;
      else if (*fmt == '0')
        zero = 1;
      else
        break;
    }
    while (*fmt >= '0' && *fmt <= '9')
      width = width * 10 + (*fmt++ - '0');
    if (*fmt == '.') {
      prec = 0;
      fmt++;
      while (*fmt >= '0' && *fmt <= '9')
        prec = prec * 10 + (*fmt++ - '0');
    }
    if (*fmt == 'l') {
      is_long = 1;
      fmt++;
    }
    conv = *fmt ? *fmt++ : 0;
    switch (conv) {
    case 's': {
      const char *s = va_arg(ap, const char *);
      int len;

      if (!s)
        s = "(null)";
      len = (int) strlen(s);
      if (prec >= 0 && len > prec)
        len = prec;
      o = put_padded(o, s, len, width, left, ' ');
      break;
    }
    case 'c':
      num[0] = (char) va_arg(ap, int);
      o = put_padded(o, num, 1, width, left, ' ');
      break;
    case 'd':
    case 'u':
    case 'x': {
      unsigned long v;
      int neg = 0;
      int base = conv == 'x' ? 16 : 10;
      int n = 0;
      char *p = num + sizeof(num);

      if (conv == 'd') {
        long sv = is_long ? va_arg(ap, long) : (long) va_arg(ap, int);

        neg = sv < 0;
        v = neg ? (unsigned long) -(sv + 1) + 1 : (unsigned long) sv;
      } else {
        v = is_long ? va_arg(ap, unsigned long)
                    : (unsigned long) va_arg(ap, unsigned int);
      }
      do {
        *--p = "0123456789abcdef"[v % (unsigned) base];
        v /= (unsigned) base;
        n++;
      } while (v);
      if (neg) {
        if (zero && !left) {
          *o++ = '-';
          width = width > 0 ? width - 1 : 0;
        } else {
          *--p = '-';
          n++;
        }
      }
      o = put_padded(o, p, n, width, left, zero && !left ? '0' : ' ');
      break;
    }
    case '%':
      *o++ = '%';
      break;
    default:
      break;
    }
  }
  *o = 0;
  return (int) (o - out);
}

int amiga_sprintf(char *out, const char *fmt, ...)
{
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = amiga_vsprintf(out, fmt, ap);
  va_end(ap);
  return n;
}
