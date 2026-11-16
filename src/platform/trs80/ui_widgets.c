#include "ui_widgets.h"
#include "trs80_console.h"
#include "trs80_keys.h"

#include <string.h>

static char linebuf[CON_COLS + 1];

static void list_scroll(list_t *l)
{
  if (l->sel < l->top)
    l->top = l->sel;
  else if (l->sel >= l->top + l->height)
    l->top = l->sel - l->height + 1;
}

void list_draw(const list_t *l)
{
  uint8_t r, n;
  uint16_t idx;
  const char *s;

  for (r = 0; r < l->height; r++) {
    idx = l->top + r;
    con_fill(l->row + r, l->col, l->width, ' ');
    if (idx >= l->count)
      continue;
    linebuf[0] = (idx == l->sel) ? '>' : ' ';
    linebuf[1] = 0;
    con_put(l->row + r, l->col, linebuf);
    s = l->item(idx, l->ctx);
    n = 0;
    while (s[n] && n < l->width - 2) {
      linebuf[n] = s[n];
      n++;
    }
    linebuf[n] = 0;
    con_put(l->row + r, l->col + 2, linebuf);
  }
}

int list_key(list_t *l, int key)
{
  uint16_t page = l->height - 1;

  if (key == KEY_UP) {
    if (l->sel == 0)
      return LIST_EDGE_UP;
    l->sel--;
  } else if (key == KEY_DOWN) {
    if (l->count == 0 || l->sel + 1 >= l->count)
      return LIST_EDGE_DOWN;
    l->sel++;
  } else if (key == KEY_SUP) {
    if (l->sel == 0)
      return LIST_EDGE_UP;
    l->sel = (l->sel > page) ? l->sel - page : 0;
  } else if (key == KEY_SDOWN) {
    if (l->count == 0 || l->sel + 1 >= l->count)
      return LIST_EDGE_DOWN;
    l->sel = (l->sel + page < l->count) ? l->sel + page : l->count - 1;
  } else {
    return LIST_NONE;
  }
  list_scroll(l);
  return LIST_MOVED;
}

static char undo[256];

static void field_draw(const char *buf, uint8_t len, uint8_t row, uint8_t col, uint8_t width)
{
  uint8_t start = (len > width - 1) ? len - (width - 1) : 0;
  uint8_t n = len - start;

  con_fill(row, col, width, ' ');
  memcpy(linebuf, buf + start, n);
  linebuf[n] = '_';
  linebuf[n + 1] = 0;
  con_put(row, col, linebuf);
}

int field_edit(char *buf, uint8_t max, uint8_t row, uint8_t col, uint8_t width)
{
  uint8_t len;
  int key;

  strcpy(undo, buf);
  len = (uint8_t) strlen(buf);
  for (;;) {
    field_draw(buf, len, row, col, width);
    con_cursor(row, col + (len > width - 1 ? width - 1 : len));
    key = con_getkey();
    if (key == KEY_ENTER)
      return 1;
    if (key == KEY_BREAK || key < 0) {
      strcpy(buf, undo);
      return 0;
    }
    if (key == KEY_BKSP) {
      if (len)
        buf[--len] = 0;
    } else if (key == KEY_SLEFT) {
      len = 0;
      buf[0] = 0;
    } else if (key >= 32 && key < 127 && len < max) {
      buf[len++] = (char) key;
      buf[len] = 0;
    }
  }
}

void tabs_draw(uint8_t row, const char *const *names, uint8_t n, uint8_t active)
{
  uint8_t i, p = 0;
  const char *s;

  con_fill(row, 0, CON_COLS, ' ');
  for (i = 0; i < n && p < CON_COLS - 16; i++) {
    linebuf[p++] = '[';
    if (i == active)
      linebuf[p++] = '*';
    for (s = names[i]; *s; s++)
      linebuf[p++] = *s;
    if (i == active)
      linebuf[p++] = '*';
    linebuf[p++] = ']';
    linebuf[p++] = ' ';
  }
  linebuf[p] = 0;
  con_put(row, 1, linebuf);
}

void msg_draw(uint8_t row, const char *text)
{
  con_fill(row, 0, CON_COLS, ' ');
  con_put(row, 1, text);
}
