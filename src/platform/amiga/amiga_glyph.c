#include "amiga_glyph.h"

#include <stddef.h>
#include <string.h>

/* [glyph][layer][row]; layer 0 is the outline (text pen), layer 1 the
 * body fill (fill pen), drawn first so the outline stays crisp. */
static const uint8_t glyphs[AMIGA_GLYPH_COUNT][2][AMIGA_GLYPH_H] = {
  /* DRAWER */
  { { 0x00, 0x70, 0xFF, 0x81, 0x81, 0x81, 0x81, 0xFF },
    { 0x00, 0x00, 0x00, 0x7E, 0x7E, 0x7E, 0x7E, 0x00 } },
  /* UP (parent) */
  { { 0x18, 0x3C, 0x7E, 0xFF, 0x18, 0x18, 0x18, 0x18 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
  /* FLOPPY: cut corner, label below */
  { { 0xFE, 0x87, 0x81, 0x81, 0x81, 0xBD, 0xBD, 0xFF },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
  /* FLOPPY_FULL */
  { { 0xFE, 0x87, 0x81, 0x81, 0x81, 0xBD, 0xBD, 0xFF },
    { 0x00, 0x78, 0x7E, 0x7E, 0x7E, 0x42, 0x42, 0x00 } },
  /* ADF: page with a disk label */
  { { 0xF8, 0x8C, 0x8A, 0x8F, 0x81, 0x81, 0x81, 0xFF },
    { 0x00, 0x00, 0x00, 0x00, 0x7E, 0x7E, 0x7E, 0x00 } },
  /* HDF: drive box with a slot and LED */
  { { 0x00, 0xFF, 0x81, 0xBD, 0x81, 0x85, 0xFF, 0x00 },
    { 0x00, 0x00, 0x7E, 0x42, 0x7E, 0x7A, 0x00, 0x00 } },
  /* FILE: page with text lines */
  { { 0xF8, 0x8C, 0x8A, 0x8F, 0xB9, 0x81, 0xB9, 0xFF },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
};

uint8_t amiga_glyph_row(uint8_t glyph, uint8_t layer, uint8_t row)
{
  uint8_t bits;

  if (glyph >= AMIGA_GLYPH_COUNT || layer > 1 || row >= AMIGA_GLYPH_H)
    return 0;
  if (glyph == AMIGA_GLYPH_FLOPPY_SAVED) {
    /* The mounted floppy with its body thinned to a checkerboard. */
    bits = glyphs[AMIGA_GLYPH_FLOPPY_FULL][layer][row];
    return layer ? (uint8_t) (bits & ((row & 1) ? 0x55 : 0xAA)) : bits;
  }
  return glyphs[glyph][layer][row];
}

/* Each art pixel becomes two screen pixels. */
static uint16_t widen(uint8_t bits)
{
  uint16_t out = 0;
  uint8_t i;

  for (i = 0; i < 8; i++) {
    out = (uint16_t) (out << 2);
    if (bits & (0x80 >> i))
      out |= 3;
  }
  return out;
}

void amiga_glyph_build(uint16_t *line, uint16_t *fill)
{
  uint16_t *out[2];
  uint8_t layer;
  uint8_t row;
  uint8_t g;

  out[0] = line;
  out[1] = fill;
  for (layer = 0; layer < 2; layer++)
    for (row = 0; row < AMIGA_GLYPH_H; row++)
      for (g = 0; g < AMIGA_GLYPH_COUNT; g++)
        out[layer][row * AMIGA_GLYPH_WORDS + g] =
          widen(amiga_glyph_row(g, layer, row));
}

/* 16x16 floppy for the drive tiles: metal shutter with a window above, a
 * label with two lines of writing below, one corner cut. */
static const char *const floppy_art[AMIGA_ICON_ART] = {
  "#############...",
  "#ooo########o#..",
  "#ooo####..##oo#.",
  "#ooo####..##ooo#",
  "#ooo####..##ooo#",
  "#ooo########ooo#",
  "#oooooooooooooo#",
  "#oooooooooooooo#",
  "#oooooooooooooo#",
  "#oo##########oo#",
  "#oo#wwwwwwww#oo#",
  "#oo#w######w#oo#",
  "#oo#wwwwwwww#oo#",
  "#oo#w#####ww#oo#",
  "#oo##########oo#",
  "################",
};

char amiga_icon_art_at(uint8_t x, uint8_t y)
{
  size_t len;

  if (x >= AMIGA_ICON_ART || y >= AMIGA_ICON_ART)
    return '.';
  len = strlen(floppy_art[y]);
  return x < len ? floppy_art[y][x] : '.';
}

/* Art pixels [x0, x0+8) of row `y` for one layer, MSB leftmost. */
static uint8_t icon_bits(char layer_ch, uint8_t x0, uint8_t y, int dotted)
{
  uint8_t bits = 0;
  uint8_t i;

  for (i = 0; i < 8; i++) {
    uint8_t x = (uint8_t) (x0 + i);

    bits = (uint8_t) (bits << 1);
    if (amiga_icon_art_at(x, y) != layer_ch)
      continue;
    /* Checkerboard thinning makes the outline read as faint. */
    if (dotted && ((x + y) & 1))
      continue;
    bits |= 1;
  }
  return bits;
}

void amiga_icon_build(uint16_t *line, uint16_t *body, uint16_t *paper)
{
  uint8_t icon;
  uint8_t row;
  uint8_t half;

  for (row = 0; row < AMIGA_ICON_H; row++) {
    for (icon = 0; icon < AMIGA_ICON_COUNT; icon++) {
      int empty = icon == AMIGA_ICON_FLOPPY_EMPTY;
      int saved = icon == AMIGA_ICON_FLOPPY_SAVED;

      for (half = 0; half < 2; half++) {
        uint16_t at = (uint16_t) (row * AMIGA_ICON_WORDS + icon * 2 + half);
        uint8_t x0 = (uint8_t) (half * 8);

        line[at] = widen(icon_bits('#', x0, row, empty));
        /* A saved (not mounted) disk keeps its outline but the body and
         * label are thinned out, so it reads as faded. */
        body[at] = empty ? 0 : widen(icon_bits('o', x0, row, saved));
        paper[at] = empty ? 0 : widen(icon_bits('w', x0, row, saved));
      }
    }
  }
}

static int ext_is(const char *dot, const char *ext)
{
  for (dot++; *ext; dot++, ext++) {
    char c = *dot;

    if (c >= 'A' && c <= 'Z')
      c = (char) (c + 32);
    if (c != *ext)
      return 0;
  }
  return *dot == 0;
}

uint8_t amiga_glyph_for_name(const char *name)
{
  const char *dot;

  if (!name)
    return AMIGA_GLYPH_FILE;
  dot = strrchr(name, '.');
  if (!dot)
    return AMIGA_GLYPH_FILE;
  if (ext_is(dot, "adf"))
    return AMIGA_GLYPH_ADF;
  if (ext_is(dot, "hdf") || ext_is(dot, "hdd"))
    return AMIGA_GLYPH_HDF;
  return AMIGA_GLYPH_FILE;
}
