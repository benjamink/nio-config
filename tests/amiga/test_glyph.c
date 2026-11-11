#include "check.h"
#include "amiga_glyph.h"

void test_glyph(void)
{
  uint16_t line[AMIGA_GLYPH_STRIP_BYTES / 2];
  uint16_t fill[AMIGA_GLYPH_STRIP_BYTES / 2];
  uint8_t g, row;

  /* 8x8 art drawn 16x8 so it is square on a hires (1:2) pixel grid. */
  CHECK(AMIGA_GLYPH_ART == 8 && AMIGA_GLYPH_H == 8 && AMIGA_GLYPH_W == 16);
  CHECK(AMIGA_GLYPH_WORDS == AMIGA_GLYPH_COUNT);   /* one word per glyph row */

  /* The strip holds each glyph's rows side by side, every art pixel
   * doubled, MSB leftmost. */
  amiga_glyph_build(line, fill);
  for (g = 0; g < AMIGA_GLYPH_COUNT; g++) {
    for (row = 0; row < AMIGA_GLYPH_H; row++) {
      uint16_t lw = line[row * AMIGA_GLYPH_WORDS + g];
      uint16_t fw = fill[row * AMIGA_GLYPH_WORDS + g];
      uint8_t lart = amiga_glyph_row(g, 0, row);
      uint8_t fart = amiga_glyph_row(g, 1, row);
      uint8_t x;

      for (x = 0; x < 8; x++) {
        unsigned want_l = (lart >> (7 - x)) & 1;
        unsigned want_f = (fart >> (7 - x)) & 1;

        CHECK(((lw >> (15 - 2 * x)) & 1) == want_l);
        CHECK(((lw >> (14 - 2 * x)) & 1) == want_l);
        CHECK(((fw >> (15 - 2 * x)) & 1) == want_f);
        CHECK(((fw >> (14 - 2 * x)) & 1) == want_f);
      }
    }
  }

  /* The fill layer sits inside the outline: no pixel is lit in the fill
   * layer of an outline-only glyph, and a mounted floppy differs from an
   * empty one only by its fill. */
  for (row = 0; row < AMIGA_GLYPH_H; row++) {
    CHECK(amiga_glyph_row(AMIGA_GLYPH_FLOPPY, 1, row) == 0);
    CHECK(amiga_glyph_row(AMIGA_GLYPH_FLOPPY, 0, row) ==
          amiga_glyph_row(AMIGA_GLYPH_FLOPPY_FULL, 0, row));
  }
  CHECK(amiga_glyph_row(AMIGA_GLYPH_FLOPPY_FULL, 1, 3) != 0);

  /* A saved (not mounted) floppy keeps the outline and thins the body. */
  {
    unsigned full = 0, thin = 0;

    for (row = 0; row < AMIGA_GLYPH_H; row++) {
      uint8_t f = amiga_glyph_row(AMIGA_GLYPH_FLOPPY_FULL, 1, row);
      uint8_t t = amiga_glyph_row(AMIGA_GLYPH_FLOPPY_SAVED, 1, row);

      CHECK(amiga_glyph_row(AMIGA_GLYPH_FLOPPY_SAVED, 0, row) ==
            amiga_glyph_row(AMIGA_GLYPH_FLOPPY_FULL, 0, row));
      CHECK((t & ~f) == 0);
      for (; f; f &= (uint8_t) (f - 1)) full++;
      for (; t; t &= (uint8_t) (t - 1)) thin++;
    }
    CHECK(thin > 0 && thin < full);
  }

  /* Out of range is blank, never a read past the table. */
  CHECK(amiga_glyph_row(AMIGA_GLYPH_COUNT, 0, 0) == 0);
  CHECK(amiga_glyph_row(0, 2, 0) == 0);
  CHECK(amiga_glyph_row(0, 0, AMIGA_GLYPH_H) == 0);

  CHECK(amiga_glyph_for_name("Lemmings.adf") == AMIGA_GLYPH_ADF);
  CHECK(amiga_glyph_for_name("LEMMINGS.ADF") == AMIGA_GLYPH_ADF);
  CHECK(amiga_glyph_for_name("Work.hdf") == AMIGA_GLYPH_HDF);
  CHECK(amiga_glyph_for_name("disk.hdd") == AMIGA_GLYPH_HDF);
  CHECK(amiga_glyph_for_name("README.txt") == AMIGA_GLYPH_FILE);
  CHECK(amiga_glyph_for_name("adf") == AMIGA_GLYPH_FILE);
  CHECK(amiga_glyph_for_name("x.adfx") == AMIGA_GLYPH_FILE);
  CHECK(amiga_glyph_for_name("") == AMIGA_GLYPH_FILE);
  CHECK(amiga_glyph_for_name(NULL) == AMIGA_GLYPH_FILE);
}
