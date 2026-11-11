#ifndef AMIGA_FORMAT_H
#define AMIGA_FORMAT_H

#include <stdint.h>

#define AMIGA_SIZE_TEXT_MAX 16
#define AMIGA_DATE_TEXT_MAX 9

/* size_format and date_format take the CONFIG_NIO_PREF_* values. */
void amiga_format_size(char *out, uint32_t size, uint8_t size_format);
int amiga_format_date(char *out, uint32_t mtime, uint8_t date_format);
void amiga_clip_head(char *out, uint16_t cap, const char *s, uint8_t max_chars);
void amiga_clip_tail(char *out, uint16_t cap, const char *s, uint8_t max_chars);
/* Copies the last non-blank line of `text` (e.g. captured command output). */
void amiga_last_line(char *out, uint16_t cap, const char *text);
/* Word-wraps `msg` into out[0..n-1], each a NUL-terminated line of at most
 * `width` characters in a `stride`-byte row (stride > width).  A '\n' forces
 * a break; a word longer than `width` is cut.  Returns n <= max_lines and
 * drops whatever does not fit. */
uint8_t amiga_wrap_text(char *out, uint8_t stride, uint8_t max_lines,
                        uint8_t width, const char *msg);

#endif
