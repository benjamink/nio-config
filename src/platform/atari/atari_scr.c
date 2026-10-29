#include "atari_scr.h"

#include <string.h>

#define DLIST ((uint8_t *) 0x0600)
#define COLOR1 (*(volatile uint8_t *) 0x02C5)
#define COLOR2 (*(volatile uint8_t *) 0x02C6)
#define COLOR4 (*(volatile uint8_t *) 0x02C8)
#define CHBAS  (*(volatile uint8_t *) 0x02F4)

/* Two screens: the display shows `front` while everything is drawn into `back`;
 * scr_present() swaps them during vertical blank, so a redraw never shows
 * half-finished and the display never wipes down the screen.
 *   scr_rowlo/hi   row addresses of the back buffer (the text routines use them)
 *   lms_lo/hi      row addresses the display list takes at the next vblank
 *   nxt_bg/fg      row colours being drawn, copied into row_bg/fg (the arrays
 *                  the display list interrupt reads) at the same moment */
uint8_t scr_rowlo[SCR_ROWS];
uint8_t scr_rowhi[SCR_ROWS];
uint8_t lms_lo[SCR_ROWS];
uint8_t lms_hi[SCR_ROWS];
uint8_t row_bg[SCR_ROWS];
uint8_t row_fg[SCR_ROWS];
uint8_t nxt_bg[SCR_ROWS];
uint8_t nxt_fg[SCR_ROWS];
uint8_t scr_sio_bg;             /* body colours the whole screen takes while SIO runs */
uint8_t scr_sio_fg;
volatile uint8_t scr_swap;      /* set by scr_present, cleared by the vblank handler */

/* ANTIC wraps a line's 40 bytes inside a 4K block, so no row may straddle a
 * 4K boundary: each buffer's rows start at its `base`, nudged by up to one row
 * to line the boundary up between two rows. */
#define BUF_SIZE (SCR_ROWS * SCR_COLS)
static uint8_t buf_raw[2][BUF_SIZE + SCR_COLS];
static uint8_t *base[2];
static uint8_t tbl_lo[2][SCR_ROWS];
static uint8_t tbl_hi[2][SCR_ROWS];
static uint8_t front;           /* 0 or 1: the buffer on display */

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

void scr_theme(uint8_t row, uint8_t theme)
{
  nxt_bg[row] = palette[theme][0];
  nxt_fg[row] = palette[theme][1];
}

static void set_back_rows(void)
{
  memcpy(scr_rowlo, tbl_lo[!front], SCR_ROWS);
  memcpy(scr_rowhi, tbl_hi[!front], SCR_ROWS);
}

void scr_init(void)
{
  uint8_t *dl;
  uint8_t r;
  uint8_t b;

  for (b = 0; b < 2; b++) {
    uint16_t raw;
    uint16_t edge;

    raw = (uint16_t) buf_raw[b];
    edge = (uint16_t) ((raw + BUF_SIZE) & 0xF000);
    if (edge > raw)
      raw = (uint16_t) (edge - SCR_COLS * ((uint16_t) (edge - raw) / SCR_COLS));
    base[b] = (uint8_t *) raw;
    memset(base[b], 0, BUF_SIZE);
    for (r = 0; r < SCR_ROWS; r++) {
      uint16_t addr;

      addr = (uint16_t) (raw + (uint16_t) r * SCR_COLS);
      tbl_lo[b][r] = (uint8_t) addr;
      tbl_hi[b][r] = (uint8_t) (addr >> 8);
    }
  }
  front = 0;
  set_back_rows();
  dl = DLIST;
  *dl++ = 0x70;
  *dl++ = 0x70;
  *dl++ = 0x70;
  for (r = 0; r < SCR_ROWS; r++) {
#ifdef NO_DLI
    *dl++ = 0x42;               /* mode 2, load address (diagnostic: no DLI) */
#else
    *dl++ = 0xC2;               /* mode 2, load address, DLI */
#endif
    *dl++ = tbl_lo[0][r];
    *dl++ = tbl_hi[0][r];
  }
  *dl++ = 0x41;                 /* jump and wait for vblank */
  *dl++ = 0x00;
  *dl = 0x06;
  for (r = 0; r < SCR_ROWS; r++)
    scr_theme(r, T_BODY);
  scr_sio_bg = palette[T_BODY][0];
  scr_sio_fg = palette[T_BODY][1];
  memcpy(row_bg, nxt_bg, SCR_ROWS);
  memcpy(row_fg, nxt_fg, SCR_ROWS);
  COLOR2 = row_bg[0];
  COLOR4 = row_bg[0];
  COLOR1 = row_fg[0];
  scr_vectors_set();
}

/* Show what has been drawn since the last call: the back buffer and the new
 * row colours are taken up by the vertical blank handler, then the buffers
 * swap roles and the new back buffer starts as a copy of what is on display. */
void scr_present(void)
{
  uint8_t r;
  unsigned wait;

  memcpy(lms_lo, scr_rowlo, SCR_ROWS);
  memcpy(lms_hi, scr_rowhi, SCR_ROWS);
  scr_swap = 1;
  for (wait = 0; scr_swap && wait < 60000U; wait++)
    ;
  front = (uint8_t) !front;
  set_back_rows();
  for (r = 0; r < SCR_ROWS; r++)
    memcpy((uint8_t *) (scr_rowlo[r] | (scr_rowhi[r] << 8)),
           (uint8_t *) (tbl_lo[front][r] | (tbl_hi[front][r] << 8)), SCR_COLS);
}

void scr_exit(void)
{
  scr_vectors_off();
}

/* Scroll bar in the last column of `rows` rows starting at first_row. */
void scr_scrollbar(uint8_t first_row, uint8_t rows, uint16_t top,
                   uint16_t visible, uint16_t total)
{
  uint8_t r;
  uint8_t track;
  uint8_t thumb_pos;
  uint8_t thumb_len;

  /* Nothing to scroll: no bar. */
  if (total <= visible)
    return;
  scr_putc(first_row, SCR_COLS - 1, G_UP);
  scr_putc((uint8_t) (first_row + rows - 1), SCR_COLS - 1, G_DN);
  track = (uint8_t) (rows - 2);
  for (r = 0; r < track; r++)
    scr_putc((uint8_t) (first_row + 1 + r), SCR_COLS - 1, '|');
  if (total <= visible)
    return;
  thumb_len = (uint8_t) ((uint16_t) (track * visible) / total);
  if (thumb_len < 1)
    thumb_len = 1;
  thumb_pos = (uint8_t) ((uint32_t) (track - thumb_len) * top /
                         (total - visible));
  for (r = 0; r < thumb_len; r++)
    scr_putc((uint8_t) (first_row + 1 + thumb_pos + r), SCR_COLS - 1, 0x80 | ' ');
}
