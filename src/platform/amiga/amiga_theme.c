#include "amiga_theme.h"

/* Workbench 1.x palette: 0 blue, 1 white, 2 black, 3 orange. */
void amiga_theme_classic(amiga_theme_t *t)
{
  t->background = 0;
  t->text = 1;
  t->shine = 1;
  t->shadow = 2;
  t->fill = 3;
  t->filltext = 2;
  t->highlight = 3;
}

static uint8_t pen(const uint16_t *pens, uint16_t count, uint16_t index,
                   uint8_t fallback)
{
  return (uint8_t) (pens && index < count ? pens[index] : fallback);
}

/* Fallbacks are the V36 default pens for a four-colour Workbench. */
void amiga_theme_from_pens(amiga_theme_t *t, const uint16_t *pens,
                           uint16_t count)
{
  t->text = pen(pens, count, AMIGA_TEXTPEN, 1);
  t->shine = pen(pens, count, AMIGA_SHINEPEN, 2);
  t->shadow = pen(pens, count, AMIGA_SHADOWPEN, 1);
  t->fill = pen(pens, count, AMIGA_FILLPEN, 3);
  t->filltext = pen(pens, count, AMIGA_FILLTEXTPEN, 1);
  t->background = pen(pens, count, AMIGA_BACKGROUNDPEN, 0);
  t->highlight = pen(pens, count, AMIGA_HIGHLIGHTTEXTPEN, 2);
}
