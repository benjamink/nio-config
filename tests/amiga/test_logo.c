#include "check.h"
#include "amiga_logo.h"

static unsigned count(const uint16_t *mask)
{
  unsigned n = 0;
  uint16_t x, y;

  for (y = 0; y < AMIGA_LOGO_H; y++)
    for (x = 0; x < AMIGA_LOGO_W; x++)
      n += (unsigned) amiga_logo_pixel(mask, x, y);
  return n;
}

void test_logo(void)
{
  uint16_t x, y;
  unsigned both = 0;
  unsigned padding = 0;

  /* 160x96 square-pixel artwork shown on 1:2 hires pixels. */
  CHECK(AMIGA_LOGO_W == 84 && AMIGA_LOGO_H == 27);
  CHECK(AMIGA_LOGO_WORDS == 6);   /* 16-bit words per row, BltTemplate */

  CHECK(count(amiga_logo_emblem) > 150);
  CHECK(count(amiga_logo_text) > 150);
  for (y = 0; y < AMIGA_LOGO_H; y++) {
    for (x = 0; x < AMIGA_LOGO_W; x++)
      both += (unsigned) (amiga_logo_pixel(amiga_logo_emblem, x, y) &&
                          amiga_logo_pixel(amiga_logo_text, x, y));
    for (x = AMIGA_LOGO_W; x < AMIGA_LOGO_WORDS * 16; x++)
      padding += (unsigned) (amiga_logo_pixel(amiga_logo_emblem, x, y) |
                             amiga_logo_pixel(amiga_logo_text, x, y));
  }
  CHECK(both == 0);       /* each pixel has one pen */
  CHECK(padding == 0);    /* row padding stays clear */

  /* The emblem is on the left, the lettering on the right. */
  CHECK(amiga_logo_pixel(amiga_logo_text, 5, 13) == 0);
  CHECK(amiga_logo_pixel(amiga_logo_emblem, 80, 5) == 0);

  /* MSB of each word is the leftmost pixel. */
  {
    static const uint16_t probe[AMIGA_LOGO_WORDS * AMIGA_LOGO_H] = {
      0x8000, 0x0001
    };
    CHECK(amiga_logo_pixel(probe, 0, 0) == 1);
    CHECK(amiga_logo_pixel(probe, 1, 0) == 0);
    CHECK(amiga_logo_pixel(probe, 31, 0) == 1);
    CHECK(amiga_logo_pixel(probe, 0, 1) == 0);
  }
}
