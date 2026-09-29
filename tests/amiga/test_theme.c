#include "check.h"
#include "amiga_theme.h"

void test_theme(void)
{
  amiga_theme_t t;
  /* Default V36 4-colour pens: detail block text shine shadow fill filltext background highlight */
  static const uint16_t v36[9] = { 0, 1, 1, 2, 1, 3, 1, 0, 2 };
  static const uint16_t custom[9] = { 0, 1, 5, 6, 7, 4, 2, 3, 6 };

  amiga_theme_classic(&t);
  CHECK(t.background == 0 && t.text == 1 && t.shine == 1 && t.shadow == 2);
  CHECK(t.fill == 3 && t.filltext == 2 && t.highlight == 3);

  amiga_theme_from_pens(&t, v36, 9);
  CHECK(t.text == 1 && t.shine == 2 && t.shadow == 1 && t.fill == 3);
  CHECK(t.filltext == 1 && t.background == 0 && t.highlight == 2);

  amiga_theme_from_pens(&t, custom, 9);
  CHECK(t.text == 5 && t.shine == 6 && t.shadow == 7 && t.fill == 4);
  CHECK(t.filltext == 2 && t.background == 3 && t.highlight == 6);

  /* A short pen array keeps the V36 defaults for missing roles. */
  amiga_theme_from_pens(&t, custom, 4);
  CHECK(t.text == 5 && t.shine == 6 && t.shadow == 1 && t.fill == 3);

  amiga_theme_from_pens(&t, NULL, 0);
  CHECK(t.text == 1 && t.shine == 2);
}
