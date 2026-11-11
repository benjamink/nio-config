#include "amiga_layout.h"

static amiga_rect_t rect(int16_t left, int16_t top, int16_t width,
                         int16_t height)
{
  amiga_rect_t r;

  r.left = left;
  r.top = top;
  r.width = width;
  r.height = height;
  return r;
}

amiga_rect_t amiga_rect_inset(amiga_rect_t r, int16_t dx, int16_t dy)
{
  return rect((int16_t) (r.left + dx), (int16_t) (r.top + dy),
              (int16_t) (r.width - 2 * dx), (int16_t) (r.height - 2 * dy));
}

/* n equal boxes separated by GAP; the last one takes the remainder. */
static void spread(amiga_rect_t *out, uint8_t n, int16_t x0, int16_t y,
                   int16_t total_w, int16_t h)
{
  int16_t w;
  uint8_t i;

  w = (int16_t) ((total_w - AMIGA_LAYOUT_GAP * (n - 1)) / n);
  for (i = 0; i < n; i++) {
    int16_t left = (int16_t) (x0 + i * (w + AMIGA_LAYOUT_GAP));
    out[i] = rect(left, y, i + 1 == n ? (int16_t) (x0 + total_w - left) : w, h);
  }
}

int amiga_layout_compute(const amiga_layout_in_t *in, amiga_layout_t *out)
{
  int16_t cw, x0, y, btn_h, str_h, info_h, status_h, list_top, fixed_below;
  int16_t label_w, slot_w, ro_w, ro_label_w, avail, rows, right, head_w;
  uint8_t gfh;

  cw = (int16_t) (AMIGA_LAYOUT_COLS * in->font_w);
  x0 = (int16_t) (in->border_l + AMIGA_LAYOUT_PAD);
  out->win_w = (uint16_t) (in->border_l + AMIGA_LAYOUT_PAD + cw +
                           AMIGA_LAYOUT_PAD + in->border_r);
  if (out->win_w > in->screen_w)
    return 0;

  gfh = in->gadget_font_h > in->font_h ? in->gadget_font_h : in->font_h;
  btn_h = (int16_t) (in->font_h + 6);
  str_h = (int16_t) (gfh + 6);
  info_h = (int16_t) (in->font_h + 2);
  status_h = (int16_t) (in->font_h + 4);
  out->row_h = (uint8_t) (in->font_h + 1);

  /* The logo shares the page-button and info rows at the right, so it
   * costs no list rows; those rows narrow to make room. */
  head_w = cw;
  y = (int16_t) (in->border_t + AMIGA_LAYOUT_PAD);
  out->logo = rect((int16_t) (x0 + cw), y, 0, 0);
  if (in->logo_w) {
    head_w = (int16_t) (cw - in->logo_w - AMIGA_LAYOUT_GAP);
    out->logo = rect((int16_t) (x0 + cw - in->logo_w), y, in->logo_w,
                     (int16_t) (btn_h + AMIGA_LAYOUT_GAP + info_h));
  }
  /* Tabs start at the page frame's left edge, AMIGA_PAGE_MARGIN outside
   * the content. */
  spread(out->tab, AMIGA_TAB_COUNT, (int16_t) (x0 - AMIGA_PAGE_MARGIN), y,
         (int16_t) (head_w + AMIGA_PAGE_MARGIN), btn_h);
  out->page = rect((int16_t) (x0 - AMIGA_PAGE_MARGIN),
                   (int16_t) (y + btn_h), (int16_t) (cw + 2 * AMIGA_PAGE_MARGIN),
                   0);
  y = (int16_t) (y + btn_h + AMIGA_LAYOUT_GAP);
  out->info = rect(x0, y, head_w, info_h);
  y = (int16_t) (y + info_h + AMIGA_LAYOUT_GAP);

  list_top = y;
  fixed_below = (int16_t) (AMIGA_LAYOUT_GAP + str_h + AMIGA_LAYOUT_GAP +
                           btn_h + AMIGA_LAYOUT_GAP + status_h +
                           AMIGA_LAYOUT_PAD + in->border_b);
  avail = (int16_t) (in->screen_h - list_top - fixed_below - 4);
  rows = (int16_t) (avail / out->row_h);
  if (rows > AMIGA_LAYOUT_MAX_ROWS)
    rows = AMIGA_LAYOUT_MAX_ROWS;
  if (rows < AMIGA_LAYOUT_MIN_ROWS)
    return 0;
  out->list_rows = (uint8_t) rows;
  out->list = rect(x0, list_top, (int16_t) (cw - AMIGA_SCROLLER_W),
                   (int16_t) (rows * out->row_h + 4));
  out->scroller = rect((int16_t) (x0 + cw - AMIGA_SCROLLER_W), list_top,
                       AMIGA_SCROLLER_W, out->list.height);
  y = (int16_t) (list_top + out->list.height + AMIGA_LAYOUT_GAP);

  /* URI label | edit | Slot label | slot | RO box | RO label */
  label_w = (int16_t) (4 * in->font_w + 4);
  slot_w = (int16_t) (3 * in->font_w + 12);
  ro_w = 18;
  ro_label_w = (int16_t) (2 * in->font_w + 4);
  right = (int16_t) (x0 + cw);
  out->ro = rect((int16_t) (right - ro_label_w - ro_w), y, ro_w, str_h);
  out->slot = rect((int16_t) (out->ro.left - AMIGA_LAYOUT_GAP - slot_w), y,
                   slot_w, str_h);
  out->edit = rect((int16_t) (x0 + label_w), y,
                   (int16_t) (out->slot.left - label_w - AMIGA_LAYOUT_GAP -
                              (x0 + label_w)),
                   str_h);
  y = (int16_t) (y + str_h + AMIGA_LAYOUT_GAP);

  spread(out->button, AMIGA_BUTTON_COUNT, x0, y, cw, btn_h);
  /* The frame's bottom edge sits one row below the buttons, in the gap
   * above the status line. */
  out->page.height = (int16_t) (y + btn_h + 2 - out->page.top);
  y = (int16_t) (y + btn_h + AMIGA_LAYOUT_GAP);
  out->status = rect(x0, y, cw, status_h);
  y = (int16_t) (y + status_h + AMIGA_LAYOUT_PAD);
  out->win_h = (uint16_t) (y + in->border_b);
  return 1;
}

int amiga_cfg_layout_compute(const amiga_layout_in_t *in,
                             amiga_cfg_layout_t *out)
{
  int16_t cw, x0, y, btn_h, info_h, status_h, fixed_below, avail, rows;

  cw = (int16_t) (AMIGA_CFG_COLS * in->font_w);
  x0 = (int16_t) (in->border_l + AMIGA_LAYOUT_PAD);
  out->win_w = (uint16_t) (in->border_l + AMIGA_LAYOUT_PAD + cw +
                           AMIGA_LAYOUT_PAD + in->border_r);
  if (out->win_w > in->screen_w)
    return 0;
  btn_h = (int16_t) (in->font_h + 6);
  info_h = (int16_t) (in->font_h + 2);
  status_h = (int16_t) (in->font_h + 4);
  out->row_h = (uint8_t) (in->font_h + 1);

  y = (int16_t) (in->border_t + AMIGA_LAYOUT_PAD);
  spread(out->tab, AMIGA_CFG_TAB_COUNT, (int16_t) (x0 - AMIGA_PAGE_MARGIN), y,
         (int16_t) (cw + 2 * AMIGA_PAGE_MARGIN), btn_h);
  out->page = rect((int16_t) (x0 - AMIGA_PAGE_MARGIN), (int16_t) (y + btn_h),
                   (int16_t) (cw + 2 * AMIGA_PAGE_MARGIN), 0);
  y = (int16_t) (y + btn_h + AMIGA_LAYOUT_GAP);
  out->info = rect(x0, y, cw, info_h);
  y = (int16_t) (y + info_h + AMIGA_LAYOUT_GAP);

  fixed_below = (int16_t) (AMIGA_LAYOUT_GAP + btn_h + AMIGA_LAYOUT_GAP +
                           status_h + AMIGA_LAYOUT_PAD + in->border_b);
  avail = (int16_t) (in->screen_h - y - fixed_below - 4);
  rows = (int16_t) (avail / out->row_h);
  if (rows > AMIGA_CFG_MAX_ROWS)
    rows = AMIGA_CFG_MAX_ROWS;
  if (rows < AMIGA_LAYOUT_MIN_ROWS)
    return 0;
  out->list_rows = (uint8_t) rows;
  out->list = rect(x0, y, (int16_t) (cw - AMIGA_SCROLLER_W),
                   (int16_t) (rows * out->row_h + 4));
  out->scroller = rect((int16_t) (x0 + cw - AMIGA_SCROLLER_W), y,
                       AMIGA_SCROLLER_W, out->list.height);
  y = (int16_t) (y + out->list.height + AMIGA_LAYOUT_GAP);

  spread(out->button, AMIGA_CFG_BUTTON_COUNT, x0, y, cw, btn_h);
  out->page.height = (int16_t) (y + btn_h + 2 - out->page.top);
  y = (int16_t) (y + btn_h + AMIGA_LAYOUT_GAP);
  out->status = rect(x0, y, cw, status_h);
  y = (int16_t) (y + status_h + AMIGA_LAYOUT_PAD);
  out->win_h = (uint16_t) (y + in->border_b);
  return 1;
}

int amiga_tiles_compute(amiga_rect_t panel, amiga_tiles_t *out)
{
  amiga_rect_t in = amiga_rect_inset(panel, AMIGA_TILE_GAP, AMIGA_TILE_GAP);
  int16_t tw, th, cap, r, c;

  out->panel = panel;
  if (in.height >= 2 * AMIGA_TILE_BIG_H + AMIGA_TILE_GAP) {
    out->big = 1;
    cap = (int16_t) (AMIGA_TILE_BIG_H + 14);
  } else if (in.height >= 2 * AMIGA_TILE_COMPACT_H + AMIGA_TILE_GAP) {
    out->big = 0;
    cap = (int16_t) (AMIGA_TILE_COMPACT_H + 4);
  } else {
    return 0;
  }
  tw = (int16_t) ((in.width - AMIGA_TILE_GAP * (AMIGA_TILE_COLS - 1)) /
                  AMIGA_TILE_COLS);
  th = (int16_t) ((in.height - AMIGA_TILE_GAP) / AMIGA_TILE_ROWS);
  if (th > cap)
    th = cap;
  if (tw < 8 * 12)
    return 0;
  /* Spare height is split above and below, so the grid sits centred. */
  in.top = (int16_t) (in.top + (in.height - (2 * th + AMIGA_TILE_GAP)) / 2);
  for (r = 0; r < AMIGA_TILE_ROWS; r++)
    for (c = 0; c < AMIGA_TILE_COLS; c++)
      out->tile[r * AMIGA_TILE_COLS + c] =
        rect((int16_t) (in.left + c * (tw + AMIGA_TILE_GAP)),
             (int16_t) (in.top + r * (th + AMIGA_TILE_GAP)), tw, th);
  return 1;
}

int amiga_tiles_hit(const amiga_tiles_t *t, int16_t x, int16_t y,
                    uint8_t *index)
{
  uint8_t i;

  for (i = 0; i < AMIGA_TILE_COUNT; i++) {
    const amiga_rect_t *r = &t->tile[i];

    if (x >= r->left && x < r->left + r->width && y >= r->top &&
        y < r->top + r->height) {
      *index = i;
      return 1;
    }
  }
  return 0;
}

uint8_t amiga_tiles_step(uint16_t selected, int dx, int dy)
{
  int c;
  int r;

  if (selected >= AMIGA_TILE_COUNT)
    return 0;
  c = (int) (selected % AMIGA_TILE_COLS) + dx;
  r = (int) (selected / AMIGA_TILE_COLS) + dy;
  if (c < 0 || c >= AMIGA_TILE_COLS || r < 0 || r >= AMIGA_TILE_ROWS)
    return (uint8_t) selected;
  return (uint8_t) (r * AMIGA_TILE_COLS + c);
}
