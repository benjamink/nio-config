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

#endif
