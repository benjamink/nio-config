#include "check.h"
#include "amiga_layout.h"

static int inside(const amiga_layout_in_t *in, const amiga_layout_t *o,
                  amiga_rect_t r)
{
  return r.left >= in->border_l && r.top >= in->border_t &&
         r.width > 0 && r.height > 0 &&
         r.left + r.width <= o->win_w - in->border_r &&
         r.top + r.height <= o->win_h - in->border_b;
}

static void check_geometry(const amiga_layout_in_t *in, const amiga_layout_t *o)
{
  int i;

  CHECK(o->win_w <= in->screen_w && o->win_h <= in->screen_h);
  for (i = 0; i < AMIGA_TAB_COUNT; i++)
    CHECK(inside(in, o, o->tab[i]));
  for (i = 0; i < AMIGA_BUTTON_COUNT; i++)
    CHECK(inside(in, o, o->button[i]));
  CHECK(inside(in, o, o->info) && inside(in, o, o->list) &&
        inside(in, o, o->scroller) && inside(in, o, o->edit) &&
        inside(in, o, o->slot) && inside(in, o, o->ro) &&
        inside(in, o, o->status));
  /* rows are stacked, never overlapping */
  CHECK(o->info.top >= o->tab[0].top + o->tab[0].height);
  CHECK(o->list.top >= o->info.top + o->info.height);
  CHECK(o->edit.top >= o->list.top + o->list.height);
  CHECK(o->button[0].top >= o->edit.top + o->edit.height);
  CHECK(o->status.top >= o->button[0].top + o->button[0].height);
  /* edit row pieces left to right, with room for their labels */
  CHECK(o->edit.left + o->edit.width + 4 * in->font_w + 4 <= o->slot.left);
  CHECK(o->slot.left + o->slot.width <= o->ro.left);
  CHECK(o->scroller.left == o->list.left + o->list.width);
  CHECK(o->list.height == o->list_rows * o->row_h + 4);
  CHECK(o->tab[0].width == o->tab[1].width);
  CHECK(o->button[0].width == o->button[4].width);
}

void test_layout(void)
{
  amiga_layout_t o;
  /* KS1.3 NTSC Workbench, topaz 8 everywhere. */
  amiga_layout_in_t ntsc13 = { 640, 200, 4, 11, 4, 2, 8, 8, 8, 0, 0 };
  /* WB3.2 PAL. */
  amiga_layout_in_t pal32 = { 640, 256, 4, 11, 4, 2, 8, 8, 8, 0, 0 };
  /* WB3.x with a 16px title font and a 13px screen font. */
  amiga_layout_in_t tall = { 640, 200, 4, 19, 4, 2, 8, 8, 13, 0, 0 };
  amiga_layout_in_t narrow = { 320, 200, 4, 11, 4, 2, 8, 8, 8, 0, 0 };
  amiga_layout_in_t shallow = { 640, 150, 4, 11, 4, 2, 8, 8, 8, 0, 0 };

  CHECK(amiga_layout_compute(&ntsc13, &o));
  CHECK(o.win_w == 624 && o.win_h == 194 && o.list_rows == 10 && o.row_h == 9);
  check_geometry(&ntsc13, &o);

  CHECK(amiga_layout_compute(&pal32, &o));
  CHECK(o.list_rows == 16 && o.win_h == 248);
  check_geometry(&pal32, &o);

  CHECK(amiga_layout_compute(&tall, &o));
  CHECK(o.list_rows >= AMIGA_LAYOUT_MIN_ROWS && o.edit.height == 19);
  check_geometry(&tall, &o);

  CHECK(!amiga_layout_compute(&narrow, &o));
  CHECK(!amiga_layout_compute(&shallow, &o));

  /* The logo sits top-right beside the page buttons and info line and
   * costs no list rows. */
  {
    amiga_layout_in_t logo = { 640, 200, 4, 11, 4, 2, 8, 8, 8, 84, 27 };

    CHECK(amiga_layout_compute(&logo, &o));
    check_geometry(&logo, &o);
    CHECK(o.list_rows == 10 && o.win_h == 194);
    CHECK(o.logo.width == 84 && o.logo.height == 27);
    CHECK(inside(&logo, &o, o.logo));
    CHECK(o.logo.left + o.logo.width == o.list.left + o.list.width +
                                        AMIGA_SCROLLER_W);
    CHECK(o.logo.top == o.tab[0].top);
    CHECK(o.logo.top + o.logo.height <= o.list.top);
    CHECK(o.tab[AMIGA_TAB_COUNT - 1].left + o.tab[AMIGA_TAB_COUNT - 1].width +
          AMIGA_LAYOUT_GAP <= o.logo.left);
    CHECK(o.info.left + o.info.width + AMIGA_LAYOUT_GAP <= o.logo.left);
  }
  /* Configuration window: fits the smallest Workbench, never overlaps,
   * and shows every Network row (10) on NTSC without scrolling. */
  {
    amiga_cfg_layout_t c;
    amiga_layout_in_t *screens[3];
    int s;
    int i;

    screens[0] = &ntsc13;
    screens[1] = &pal32;
    screens[2] = &tall;
    for (s = 0; s < 3; s++) {
      CHECK(amiga_cfg_layout_compute(screens[s], &c));
      CHECK(c.win_w <= screens[s]->screen_w && c.win_h <= screens[s]->screen_h);
      CHECK(c.list_rows >= 10 && c.list_rows <= AMIGA_CFG_MAX_ROWS);
      CHECK(c.info.top >= c.tab[0].top + c.tab[0].height);
      CHECK(c.list.top >= c.info.top + c.info.height);
      CHECK(c.button[0].top >= c.list.top + c.list.height);
      CHECK(c.status.top >= c.button[0].top + c.button[0].height);
      CHECK(c.scroller.left == c.list.left + c.list.width);
      CHECK(c.list.height == c.list_rows * c.row_h + 4);
      CHECK(c.tab[1].width - c.tab[0].width <= 1);   /* remainder to the last */
      for (i = 0; i < AMIGA_CFG_BUTTON_COUNT; i++)
        CHECK(c.button[i].left + c.button[i].width <=
              c.status.left + c.status.width);
      CHECK(c.win_h == c.status.top + c.status.height + AMIGA_LAYOUT_PAD +
                       screens[s]->border_b);
    }
    CHECK(amiga_cfg_layout_compute(&ntsc13, &c) && c.list_rows == 12);
    CHECK(!amiga_cfg_layout_compute(&narrow, &c));
  }

  /* Without a logo the header spans the full width. */
  CHECK(amiga_layout_compute(&ntsc13, &o));
  CHECK(o.logo.width == 0);
  CHECK(o.info.width == o.list.width + AMIGA_SCROLLER_W);
}
