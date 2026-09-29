#include "amiga_logo.h"

int amiga_logo_pixel(const uint16_t *mask, uint16_t x, uint16_t y)
{
  uint16_t word;

  if (x >= AMIGA_LOGO_WORDS * 16 || y >= AMIGA_LOGO_H)
    return 0;
  word = mask[y * AMIGA_LOGO_WORDS + x / 16];
  return (word >> (15 - (x % 16))) & 1;
}
