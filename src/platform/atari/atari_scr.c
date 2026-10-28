#include "atari_scr.h"

#include <string.h>

#define DLIST ((uint8_t *) 0x0600)
#define COLOR1 (*(volatile uint8_t *) 0x02C5)
#define COLOR2 (*(volatile uint8_t *) 0x02C6)
#define COLOR4 (*(volatile uint8_t *) 0x02C8)
#define CHBAS  (*(volatile uint8_t *) 0x02F4)

uint8_t row_bg[SCR_ROWS];
uint8_t row_fg[SCR_ROWS];

/* ANTIC wraps a line's 40 bytes inside a 4K block, so no row may straddle a
 * 4K boundary: the rows start at `screen`, nudged by up to one row to line
 * the boundary up between two rows. */
static uint8_t screen_raw[SCR_ROWS * SCR_COLS + SCR_COLS];
static uint8_t *screen;

/* Background hue/luminance and text luminance.  In ANTIC mode 2 the text
 * takes its hue from the background colour, so each scheme is one hue. */
static const uint8_t palette[T_COUNT][2] = {
  { 0x84, 0x0F },   /* title: brighter blue band          */
  { 0x72, 0x0A },   /* tab strip                          */
  { 0x00, 0x0E },   /* URI / path field: black            */
  { 0x74, 0x0E },   /* body: the blue screen              */
  { 0x2A, 0x00 },   /* selection: orange bar              */
  { 0x70, 0x08 },   /* info line                          */
  { 0x72, 0x0C },   /* key hints                          */
  { 0x04, 0x0E },   /* status: grey                       */
  { 0x34, 0x0E },   /* error: red                         */
  { 0x94, 0x0E },   /* dialog                             */
  { 0xC6, 0x00 }    /* edit field: green                  */
};

static uint8_t to_screen(uint8_t c)
{
  uint8_t inv;

  inv = (uint8_t) (c & 0x80);
  c &= 0x7F;
  if (c < 32)                   /* ATASCII graphics sit at screen codes 64-95 */
    return (uint8_t) ((c + 64) | inv);
  if (c < 96)
    return (uint8_t) ((c - 32) | inv);
  return (uint8_t) (c | inv);
}

void scr_theme(uint8_t row, uint8_t theme)
{
  row_bg[row] = palette[theme][0];
  row_fg[row] = palette[theme][1];
  if (row == 0) {
    COLOR2 = row_bg[0];
    COLOR4 = row_bg[0];
    COLOR1 = row_fg[0];
  }
}

void scr_init(void)
{
  uint8_t *dl;
  uint8_t r;

  {
    uint16_t raw;
    uint16_t edge;

    raw = (uint16_t) screen_raw;
    edge = (uint16_t) ((raw + SCR_ROWS * SCR_COLS) & 0xF000);
    if (edge > raw)
      raw = (uint16_t) (edge - SCR_COLS * ((uint16_t) (edge - raw) / SCR_COLS));
    screen = (uint8_t *) raw;
  }
  memset(screen, 0, SCR_ROWS * SCR_COLS);
  dl = DLIST;
  *dl++ = 0x70;
  *dl++ = 0x70;
  *dl++ = 0x70;
  for (r = 0; r < SCR_ROWS; r++) {
    uint16_t addr;

    addr = (uint16_t) &screen[(uint16_t) r * SCR_COLS];
    *dl++ = 0xC2;               /* mode 2, load address, DLI */
    *dl++ = (uint8_t) addr;
    *dl++ = (uint8_t) (addr >> 8);
  }
  *dl++ = 0x41;                 /* jump and wait for vblank */
  *dl++ = 0x00;
  *dl = 0x06;
  for (r = 0; r < SCR_ROWS; r++)
    scr_theme(r, T_BODY);
  scr_vectors_set();
}

void scr_exit(void)
{
  scr_vectors_off();
}

void scr_clear(void)
{
  memset(screen, 0, SCR_ROWS * SCR_COLS);
}

void scr_fill(uint8_t row, uint8_t col, uint8_t len, char ch)
{
  memset(&screen[(uint16_t) row * SCR_COLS + col], to_screen((uint8_t) ch), len);
}

void scr_putc(uint8_t row, uint8_t col, char ch)
{
  screen[(uint16_t) row * SCR_COLS + col] = to_screen((uint8_t) ch);
}

static void text(uint8_t row, uint8_t col, const char *s, uint8_t width,
                 uint8_t inv)
{
  uint8_t *p;
  uint8_t i;

  p = &screen[(uint16_t) row * SCR_COLS + col];
  for (i = 0; i < width; i++) {
    uint8_t c;

    c = *s ? (uint8_t) *s++ : (uint8_t) ' ';
    *p++ = (uint8_t) (to_screen(c) ^ inv);
  }
}

void scr_text(uint8_t row, uint8_t col, const char *s, uint8_t width)
{
  text(row, col, s, width, 0);
}

void scr_text_inv(uint8_t row, uint8_t col, const char *s, uint8_t width)
{
  text(row, col, s, width, 0x80);
}

void scr_uint(uint8_t row, uint8_t col, unsigned long v, uint8_t width)
{
  char buf[12];
  uint8_t i;

  i = sizeof(buf) - 1;
  buf[i] = 0;
  do {
    buf[--i] = (char) ('0' + (uint8_t) (v % 10));
    v /= 10;
  } while (v && i);
  /* right-aligned in width */
  {
    uint8_t len;
    uint8_t pad;

    len = (uint8_t) (sizeof(buf) - 1 - i);
    pad = len < width ? (uint8_t) (width - len) : 0;
    scr_fill(row, col, pad, ' ');
    scr_text(row, (uint8_t) (col + pad), &buf[i], (uint8_t) (width - pad));
  }
}

/* Scroll bar in the last column of `rows` rows starting at first_row. */
void scr_scrollbar(uint8_t first_row, uint8_t rows, uint16_t top,
                   uint16_t visible, uint16_t total)
{
  uint8_t r;
  uint8_t track;
  uint8_t thumb_pos;
  uint8_t thumb_len;

  scr_putc(first_row, SCR_COLS - 1, G_UP);
  scr_putc((uint8_t) (first_row + rows - 1), SCR_COLS - 1, G_DN);
  track = (uint8_t) (rows - 2);
  for (r = 0; r < track; r++)
    scr_putc((uint8_t) (first_row + 1 + r), SCR_COLS - 1, '|');
  if (total <= visible)
    return;
  thumb_len = (uint8_t) ((uint32_t) track * visible / total);
  if (thumb_len < 1)
    thumb_len = 1;
  thumb_pos = (uint8_t) ((uint32_t) (track - thumb_len) * top /
                         (total - visible));
  for (r = 0; r < thumb_len; r++)
    scr_putc((uint8_t) (first_row + 1 + thumb_pos + r), SCR_COLS - 1, 0x80 | ' ');
}
