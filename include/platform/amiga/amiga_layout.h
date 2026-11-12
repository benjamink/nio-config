#ifndef AMIGA_LAYOUT_H
#define AMIGA_LAYOUT_H

#include <stdint.h>

#define AMIGA_LAYOUT_PAD 4
#define AMIGA_LAYOUT_GAP 3
#define AMIGA_LAYOUT_COLS 76
#define AMIGA_PAGE_MARGIN 3   /* page frame outside the content, each side */
#define AMIGA_LAYOUT_MIN_ROWS 6
#define AMIGA_LAYOUT_MAX_ROWS 16
#define AMIGA_SCROLLER_W 16
#define AMIGA_TAB_COUNT 4
#define AMIGA_BUTTON_COUNT 6
/* The longest label on a page button, in characters.  Buttons share the
 * row, so a label must leave room for the bevel and a pixel of padding. */
#define AMIGA_BUTTON_LABEL_MAX 10

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
  /* Frame round the page: its top edge is the tab ledge, directly under
   * the tabs, and it reaches down to just below the button row. */
  amiga_rect_t page;
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

/* Drive tiles: the Drives page shows its eight drives as a grid instead of
 * a list.  Tiles are "big" (icon, name, mode: 46+ pixels high) when the
 * panel has the room, "compact" (glyph and name on two lines) when it has
 * less, and the page stays a list when even that does not fit. */
#define AMIGA_TILE_COLS 4
#define AMIGA_TILE_ROWS 2
#define AMIGA_TILE_COUNT (AMIGA_TILE_COLS * AMIGA_TILE_ROWS)
#define AMIGA_TILE_BIG_H 46
#define AMIGA_TILE_COMPACT_H 26
#define AMIGA_TILE_GAP 4

typedef struct {
  uint8_t big;                         /* 1 big tiles, 0 compact */
  amiga_rect_t panel;
  amiga_rect_t tile[AMIGA_TILE_COUNT]; /* row-major: DN0..3 over DN4..7 */
} amiga_tiles_t;

/* Lays the tiles inside `panel`; returns 0 when it is too small. */
int amiga_tiles_compute(amiga_rect_t panel, amiga_tiles_t *out);
/* Tile under a window-relative point. */
int amiga_tiles_hit(const amiga_tiles_t *t, int16_t x, int16_t y,
                    uint8_t *index);
/* Moves the selection one tile; stays put at an edge.  `none` starts at 0. */
uint8_t amiga_tiles_step(uint16_t selected, int dx, int dy);

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
  amiga_rect_t page;       /* frame round the page, as in amiga_layout_t */
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
