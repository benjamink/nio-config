#ifndef AMIGA_THEME_H
#define AMIGA_THEME_H

#include <stdint.h>

/* DrawInfo pen indices, copied from intuition/screens.h so pure code does
 * not need the Amiga headers. */
enum {
  AMIGA_DETAILPEN = 0,
  AMIGA_BLOCKPEN,
  AMIGA_TEXTPEN,
  AMIGA_SHINEPEN,
  AMIGA_SHADOWPEN,
  AMIGA_FILLPEN,
  AMIGA_FILLTEXTPEN,
  AMIGA_BACKGROUNDPEN,
  AMIGA_HIGHLIGHTTEXTPEN
};

typedef struct {
  uint8_t text;
  uint8_t shine;
  uint8_t shadow;
  uint8_t fill;
  uint8_t filltext;
  uint8_t background;
  uint8_t highlight;
} amiga_theme_t;

void amiga_theme_classic(amiga_theme_t *t);
void amiga_theme_from_pens(amiga_theme_t *t, const uint16_t *pens,
                           uint16_t count);
/* Pens for the running Intuition when DrawInfo is not read (the WB1.3
 * build): the 1.x palette before V36, the V36 default pens from V36 on,
 * where pens 1 and 2 swap roles (black text and shadow, white shine). */
void amiga_theme_for_version(amiga_theme_t *t, uint16_t intuition_version);

#endif
