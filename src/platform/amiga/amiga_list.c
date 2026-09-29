#include "amiga_list.h"

static void clamp_top(amiga_list_t *l)
{
  uint16_t max_top;

  max_top = l->count > l->rows ? (uint16_t) (l->count - l->rows) : 0;
  if (l->top > max_top)
    l->top = max_top;
}

static void ensure_visible(amiga_list_t *l)
{
  if (l->selected == AMIGA_LIST_NONE)
    return;
  if (l->selected < l->top)
    l->top = l->selected;
  else if (l->selected >= (uint16_t) (l->top + l->rows))
    l->top = (uint16_t) (l->selected - l->rows + 1);
}

void amiga_list_init(amiga_list_t *l, uint8_t rows)
{
  l->count = 0;
  l->top = 0;
  l->selected = AMIGA_LIST_NONE;
  l->rows = rows ? rows : 1;
}

void amiga_list_set_rows(amiga_list_t *l, uint8_t rows)
{
  l->rows = rows ? rows : 1;
  clamp_top(l);
  ensure_visible(l);
}

void amiga_list_set_count(amiga_list_t *l, uint16_t count)
{
  l->count = count;
  if (count == 0)
    l->selected = AMIGA_LIST_NONE;
  else if (l->selected == AMIGA_LIST_NONE)
    l->selected = 0;
  else if (l->selected >= count)
    l->selected = (uint16_t) (count - 1);
  clamp_top(l);
  ensure_visible(l);
}

void amiga_list_select(amiga_list_t *l, uint16_t index)
{
  if (l->count == 0)
    return;
  l->selected = index >= l->count ? (uint16_t) (l->count - 1) : index;
  ensure_visible(l);
}

void amiga_list_move(amiga_list_t *l, int16_t delta)
{
  int32_t target;

  if (l->count == 0)
    return;
  target = (int32_t) (l->selected == AMIGA_LIST_NONE ? 0 : l->selected) + delta;
  if (target < 0)
    target = 0;
  if (target >= (int32_t) l->count)
    target = (int32_t) l->count - 1;
  amiga_list_select(l, (uint16_t) target);
}

int amiga_list_hit(const amiga_list_t *l, int16_t y, uint8_t row_h,
                   uint16_t *index)
{
  uint16_t row;
  uint32_t idx;

  if (y < 0 || row_h == 0)
    return 0;
  row = (uint16_t) (y / row_h);
  if (row >= l->rows)
    return 0;
  idx = (uint32_t) l->top + row;
  if (idx >= l->count)
    return 0;
  *index = (uint16_t) idx;
  return 1;
}

void amiga_list_prop(const amiga_list_t *l, uint16_t *pot, uint16_t *body)
{
  uint16_t hidden;

  if (l->count <= l->rows) {
    *pot = 0;
    *body = 0xFFFF;
    return;
  }
  hidden = (uint16_t) (l->count - l->rows);
  *body = (uint16_t) (((uint32_t) l->rows * 0xFFFFUL) / l->count);
  *pot = (uint16_t) (((uint32_t) l->top * 0xFFFFUL) / hidden);
}

void amiga_list_set_top_from_pot(amiga_list_t *l, uint16_t pot)
{
  uint16_t hidden;

  if (l->count <= l->rows) {
    l->top = 0;
    return;
  }
  hidden = (uint16_t) (l->count - l->rows);
  l->top = (uint16_t) (((uint32_t) pot * hidden + 0x7FFFUL) / 0xFFFFUL);
  clamp_top(l);
}
