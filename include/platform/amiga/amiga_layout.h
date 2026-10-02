#ifndef AMIGA_LAYOUT_H
#define AMIGA_LAYOUT_H

#include <stdint.h>

#define AMIGA_LAYOUT_PAD 4
#define AMIGA_LAYOUT_GAP 3
#define AMIGA_LAYOUT_COLS 76
#define AMIGA_LAYOUT_MIN_ROWS 6
#define AMIGA_LAYOUT_MAX_ROWS 16
#define AMIGA_SCROLLER_W 16
#define AMIGA_TAB_COUNT 4
#define AMIGA_BUTTON_COUNT 6

typedef struct {
  int16_t left;
  int16_t top;
  int16_t width;
  int16_t height;
} amiga_rect_t;

typedef struct {
  uint16_t screen_w, screen_h;
  uint8_t border_l, border_t, border_r, border_b; /* the opened window's real borders */
  uint8_t font_w, font_h;                         /* rendering font (topaz 8 = 8, 8) */
  uint8_t gadget_font_h;                          /* font string gadgets draw with */
  uint8_t logo_w, logo_h;                         /* 0,0 = no logo */
} amiga_layout_in_t;

/* Rectangles are frames; the GUI insets gadgets inside them. */
typedef struct {
  uint16_t win_w, win_h;
  uint8_t row_h, list_rows;
  amiga_rect_t tab[AMIGA_TAB_COUNT];
  amiga_rect_t logo;       /* top right, beside tabs and info; 0 width = none */
  amiga_rect_t info;
  amiga_rect_t list;
  amiga_rect_t scroller;
  amiga_rect_t edit;
  amiga_rect_t slot;
  amiga_rect_t ro;
  amiga_rect_t button[AMIGA_BUTTON_COUNT];
  amiga_rect_t status;
} amiga_layout_t;

/* Returns 0 when the screen is too small for the window. */
int amiga_layout_compute(const amiga_layout_in_t *in, amiga_layout_t *out);

/* Configuration window (Settings > Configure): tabs, an info line, a list
 * with scroller, a button row and a status line.  No logo, no editors. */
#define AMIGA_CFG_COLS 60
#define AMIGA_CFG_TAB_COUNT 2
#define AMIGA_CFG_BUTTON_COUNT 5
#define AMIGA_CFG_MAX_ROWS 12

typedef struct {
  uint16_t win_w, win_h;
  uint8_t row_h, list_rows;
  amiga_rect_t tab[AMIGA_CFG_TAB_COUNT];
  amiga_rect_t info;
  amiga_rect_t list;
  amiga_rect_t scroller;
  amiga_rect_t button[AMIGA_CFG_BUTTON_COUNT];
  amiga_rect_t status;
} amiga_cfg_layout_t;

int amiga_cfg_layout_compute(const amiga_layout_in_t *in,
                             amiga_cfg_layout_t *out);
amiga_rect_t amiga_rect_inset(amiga_rect_t r, int16_t dx, int16_t dy);

#endif
