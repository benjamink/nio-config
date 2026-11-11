#ifndef AMIGA_GLYPH_INCLUDED
#define AMIGA_GLYPH_INCLUDED

#include <stdint.h>

/* Row glyphs (drawer, floppy, disk image ...) for the list pages.  The art
 * is 8x8 logical pixels, drawn 16x8 on screen: the window is hires, where a
 * pixel is twice as tall as it is wide, so doubling the width keeps the
 * glyphs square instead of squished.
 * Each glyph has two 1-bit layers: `line` is drawn in the text pen and
 * `fill` in the fill pen, so the art uses only the screen's own pens and
 * works on a four-colour Workbench 1.3 as well as 2.x/3.x. */
#define AMIGA_GLYPH_ART 8   /* logical pixels per row of art */
#define AMIGA_GLYPH_W 16    /* screen pixels: each art pixel is two wide */
#define AMIGA_GLYPH_H 8

enum {
  AMIGA_GLYPH_DRAWER = 0,
  AMIGA_GLYPH_UP,
  AMIGA_GLYPH_FLOPPY,        /* empty / not mounted: outline only */
  AMIGA_GLYPH_FLOPPY_FULL,   /* mounted: outline and filled body */
  AMIGA_GLYPH_ADF,
  AMIGA_GLYPH_HDF,
  AMIGA_GLYPH_FILE,
  AMIGA_GLYPH_FLOPPY_SAVED,  /* mapped but not mounted: ghosted body */
  AMIGA_GLYPH_COUNT
};

/* The glyphs sit side by side in one strip, one 16-bit word each per row,
 * so a template row is AMIGA_GLYPH_WORDS words. */
#define AMIGA_GLYPH_WORDS AMIGA_GLYPH_COUNT
#define AMIGA_GLYPH_STRIP_BYTES (AMIGA_GLYPH_WORDS * 2 * AMIGA_GLYPH_H)

/* Fills two strips of AMIGA_GLYPH_STRIP_BYTES bytes each (word aligned;
 * the GUI passes chip RAM so BltTemplate can read them).  The MSB of a
 * word is its leftmost pixel. */
void amiga_glyph_build(uint16_t *line, uint16_t *fill);

/* One row of art (8 pixels, MSB leftmost) of a glyph's layer; 0 if out of range. */
uint8_t amiga_glyph_row(uint8_t glyph, uint8_t layer, uint8_t row);

/* Glyph for a file name by extension: .adf, .hdf and .hdd images get
 * their own art, anything else is a plain file. */
uint8_t amiga_glyph_for_name(const char *name);

/* Large drive-tile icons: 16x16 art drawn 32x16 on screen.  Three layers so
 * the art needs only the screen's own pens: `line` in the text pen, `body`
 * in the fill pen and `paper` (the label) in the shine pen.  A mounted
 * floppy is solid; an empty one is a dotted outline only. */
#define AMIGA_ICON_ART 16
#define AMIGA_ICON_W 32
#define AMIGA_ICON_H 16

enum {
  AMIGA_ICON_FLOPPY_FULL = 0,
  AMIGA_ICON_FLOPPY_EMPTY,
  AMIGA_ICON_FLOPPY_SAVED,   /* mapped but not mounted: solid outline, ghosted body */
  AMIGA_ICON_COUNT
};

/* One strip per layer, two words (32 pixels) per icon per row. */
#define AMIGA_ICON_WORDS (AMIGA_ICON_COUNT * 2)
#define AMIGA_ICON_STRIP_BYTES (AMIGA_ICON_WORDS * 2 * AMIGA_ICON_H)

void amiga_icon_build(uint16_t *line, uint16_t *body, uint16_t *paper);

/* The art itself: '#' outline, 'o' body, 'w' label paper, '.' clear. */
char amiga_icon_art_at(uint8_t x, uint8_t y);

#endif
