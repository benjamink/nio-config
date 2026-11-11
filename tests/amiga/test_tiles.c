#include "check.h"
#include "amiga_layout.h"
#include "amiga_glyph.h"
#include "amiga_input.h"

static amiga_rect_t panel(int16_t w, int16_t h)
{
  amiga_rect_t r;

  r.left = 10;
  r.top = 40;
  r.width = w;
  r.height = h;
  return r;
}

void test_tiles(void)
{
  amiga_tiles_t t;
  uint8_t i, j, hit;
  uint16_t line[AMIGA_ICON_STRIP_BYTES / 2];
  uint16_t body[AMIGA_ICON_STRIP_BYTES / 2];
  uint16_t paper[AMIGA_ICON_STRIP_BYTES / 2];

  /* Tall panel (a PAL window): big tiles, all inside the panel. */
  CHECK(amiga_tiles_compute(panel(608, 148), &t) == 1);
  CHECK(t.big == 1);
  for (i = 0; i < AMIGA_TILE_COUNT; i++) {
    CHECK(t.tile[i].left >= t.panel.left &&
          t.tile[i].left + t.tile[i].width <= t.panel.left + t.panel.width);
    CHECK(t.tile[i].top >= t.panel.top &&
          t.tile[i].top + t.tile[i].height <= t.panel.top + t.panel.height);
    CHECK(t.tile[i].height >= AMIGA_TILE_BIG_H);
    /* Tiles never overlap. */
    for (j = (uint8_t) (i + 1); j < AMIGA_TILE_COUNT; j++) {
      const amiga_rect_t *a = &t.tile[i];
      const amiga_rect_t *b = &t.tile[j];

      CHECK(a->left + a->width <= b->left || b->left + b->width <= a->left ||
            a->top + a->height <= b->top || b->top + b->height <= a->top);
    }
  }
  /* Row-major: DN0..3 across the top, DN4..7 below. */
  CHECK(t.tile[1].left > t.tile[0].left && t.tile[1].top == t.tile[0].top);
  CHECK(t.tile[4].top > t.tile[0].top && t.tile[4].left == t.tile[0].left);

  /* Shorter panel: compact tiles; shorter still: stay a list. */
  CHECK(amiga_tiles_compute(panel(608, 80), &t) == 1 && t.big == 0);
  CHECK(amiga_tiles_compute(panel(608, 50), &t) == 0);
  CHECK(amiga_tiles_compute(panel(200, 148), &t) == 0);   /* too narrow */

  /* Hit testing: centre of a tile finds it, the gap and outside do not. */
  CHECK(amiga_tiles_compute(panel(608, 148), &t) == 1);
  for (i = 0; i < AMIGA_TILE_COUNT; i++) {
    hit = 99;
    CHECK(amiga_tiles_hit(&t, (int16_t) (t.tile[i].left + t.tile[i].width / 2),
                          (int16_t) (t.tile[i].top + t.tile[i].height / 2),
                          &hit) == 1);
    CHECK(hit == i);
  }
  CHECK(amiga_tiles_hit(&t, (int16_t) (t.tile[0].left + t.tile[0].width + 1),
                        (int16_t) (t.tile[0].top + 5), &hit) == 0);
  CHECK(amiga_tiles_hit(&t, 0, 0, &hit) == 0);

  /* Keyboard stepping stays on the grid. */
  CHECK(amiga_tiles_step(0xFFFFu, 1, 0) == 0);
  CHECK(amiga_tiles_step(0, 1, 0) == 1);
  CHECK(amiga_tiles_step(3, 1, 0) == 3);    /* right edge */
  CHECK(amiga_tiles_step(0, -1, 0) == 0);   /* left edge */
  CHECK(amiga_tiles_step(1, 0, 1) == 5);
  CHECK(amiga_tiles_step(5, 0, -1) == 1);
  CHECK(amiga_tiles_step(5, 0, 1) == 5);    /* bottom row */
  CHECK(amiga_tiles_step(1, 0, -1) == 1);   /* top row */
  CHECK(amiga_tiles_step(4, 1, 0) == 5);

  /* Cursor keys reach the tiles. */
  CHECK(amiga_key_from_raw(0x4F, 0) == AMIGA_KEY_LEFT);
  CHECK(amiga_key_from_raw(0x4E, 0) == AMIGA_KEY_RIGHT);

  /* Icon art: 16x16, a mounted floppy has body and paper, an empty one is
   * a sparse outline only. */
  CHECK(AMIGA_ICON_W == 2 * AMIGA_ICON_ART && AMIGA_ICON_H == AMIGA_ICON_ART);
  CHECK(amiga_icon_art_at(0, 15) == '#' && amiga_icon_art_at(15, 15) == '#');
  CHECK(amiga_icon_art_at(15, 0) == '.');    /* cut corner */
  CHECK(amiga_icon_art_at(5, 6) == 'o');
  CHECK(amiga_icon_art_at(5, 10) == 'w');
  CHECK(amiga_icon_art_at(16, 0) == '.' && amiga_icon_art_at(0, 16) == '.');
  amiga_icon_build(line, body, paper);
  {
    unsigned full_body = 0, empty_body = 0, full_paper = 0, empty_paper = 0;
    unsigned full_line = 0, empty_line = 0;
    uint8_t row, w;

    for (row = 0; row < AMIGA_ICON_H; row++) {
      for (w = 0; w < 2; w++) {
        uint16_t f = (uint16_t) (row * AMIGA_ICON_WORDS + w);
        uint16_t e = (uint16_t) (row * AMIGA_ICON_WORDS + 2 + w);
        uint16_t v;

        for (v = line[f]; v; v &= (uint16_t) (v - 1)) full_line++;
        for (v = line[e]; v; v &= (uint16_t) (v - 1)) empty_line++;
        for (v = body[f]; v; v &= (uint16_t) (v - 1)) full_body++;
        for (v = body[e]; v; v &= (uint16_t) (v - 1)) empty_body++;
        for (v = paper[f]; v; v &= (uint16_t) (v - 1)) full_paper++;
        for (v = paper[e]; v; v &= (uint16_t) (v - 1)) empty_paper++;
      }
    }
    CHECK(full_body > 100 && full_paper > 30 && full_line > 100);
    CHECK(empty_body == 0 && empty_paper == 0);
    CHECK(empty_line > 0 && empty_line < full_line);   /* dotted outline */
  }
  /* A saved (not mounted) disk: the full outline, a thinned body and label. */
  {
    unsigned s_line = 0, s_body = 0, s_paper = 0, f_line = 0, f_body = 0;
    unsigned f_paper = 0;
    uint8_t row, w;

    for (row = 0; row < AMIGA_ICON_H; row++) {
      for (w = 0; w < 2; w++) {
        uint16_t f = (uint16_t) (row * AMIGA_ICON_WORDS + w);
        uint16_t sv = (uint16_t) (row * AMIGA_ICON_WORDS +
                                  2 * AMIGA_ICON_FLOPPY_SAVED + w);
        uint16_t v;

        for (v = line[f]; v; v &= (uint16_t) (v - 1)) f_line++;
        for (v = line[sv]; v; v &= (uint16_t) (v - 1)) s_line++;
        for (v = body[f]; v; v &= (uint16_t) (v - 1)) f_body++;
        for (v = body[sv]; v; v &= (uint16_t) (v - 1)) s_body++;
        for (v = paper[f]; v; v &= (uint16_t) (v - 1)) f_paper++;
        for (v = paper[sv]; v; v &= (uint16_t) (v - 1)) s_paper++;
      }
    }
    CHECK(s_line == f_line);
    CHECK(s_body > 0 && s_body < f_body && s_paper > 0 && s_paper < f_paper);
  }
}
