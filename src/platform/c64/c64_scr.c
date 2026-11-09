#include "c64_scr.h"

#include <string.h>

#define SCREEN ((uint8_t *) 0x0400)
#define COLORS ((uint8_t *) 0xD800)
#define VIC_BORDER (*(volatile uint8_t *) 0xD020)
#define VIC_BACKGROUND (*(volatile uint8_t *) 0xD021)
#define VIC_MEMORY (*(volatile uint8_t *) 0xD018)
#define KERNAL_SHIFT_LOCK (*(volatile uint8_t *) 0x0291)
#define PROCESSOR_PORT (*(volatile uint8_t *) 0x0001)

#define COLOR_BLACK 0
#define COLOR_WHITE 1
#define COLOR_RED 2
#define COLOR_CYAN 3
#define COLOR_GREEN 5
#define COLOR_BLUE 6
#define COLOR_YELLOW 7
#define COLOR_ORANGE 8
#define COLOR_LIGHT_RED 10
#define COLOR_GREY 12
#define COLOR_LIGHT_GREEN 13
#define COLOR_LIGHT_BLUE 14
#define COLOR_LIGHT_GREY 15

#define INVERT 0x80

/* Text colour for each scheme. The VIC-II has one background, so a row that the
 * Atari shows as a colour bar (the selection, an error) is reverse video here. */
static const uint8_t theme_color[T_COUNT] = {
  COLOR_WHITE,       /* title           */
  COLOR_LIGHT_BLUE,  /* tab strip       */
  COLOR_YELLOW,      /* URI / path      */
  COLOR_LIGHT_GREY,  /* body            */
  COLOR_ORANGE,      /* selection       */
  COLOR_GREY,        /* info line       */
  COLOR_LIGHT_BLUE,  /* key hints       */
  COLOR_LIGHT_GREEN, /* status          */
  COLOR_LIGHT_RED,   /* error           */
  COLOR_CYAN,        /* dialog          */
  COLOR_LIGHT_GREEN  /* edit field      */
};

static const uint8_t theme_invert[T_COUNT] = {
  INVERT, /* title */
  0, 0, 0,
  INVERT, /* selection */
  0, 0, 0,
  INVERT, /* error */
  0,
  INVERT  /* edit field */
};

/* The rows' current reverse-video state: scr_theme can change it after a row is
 * drawn, as the Atari's row colours can. */
static uint8_t row_invert[SCR_ROWS];

/* ASCII to screen code in the lower/upper case character set, and the glyphs in
 * bytes 1..31. Filled in by scr_init. */
static uint8_t to_screen[128];

static uint8_t *cell(uint8_t row, uint8_t col)
{
  return SCREEN + (unsigned) row * SCR_COLS + col;
}

static void build_table(void)
{
  uint8_t c;

  for (c = 0; c < 128; c++) {
    if (c >= 'a' && c <= 'z')
      to_screen[c] = (uint8_t) (c - 'a' + 1);
    else if (c >= 'A' && c <= 'Z')
      to_screen[c] = c;                  /* 0x41-0x5A: capitals in this set */
    else if (c >= 0x20 && c <= 0x3F)
      to_screen[c] = c;
    else
      to_screen[c] = 0x20;
  }
  to_screen['@'] = 0x00;
  to_screen['['] = 0x1B;
  to_screen['\\'] = 0x1C;                /* the pound sign is the nearest */
  to_screen[']'] = 0x1D;
  to_screen['^'] = 0x1E;
  to_screen['_'] = 0x64;
  to_screen['`'] = 0x27;
  to_screen['{'] = 0x1B;
  to_screen['|'] = 0x5D;
  to_screen['}'] = 0x1D;
  to_screen['~'] = 0x2D;

  to_screen[G_TL] = 0x70;
  to_screen[G_TR] = 0x6E;
  to_screen[G_BL] = 0x6D;
  to_screen[G_BR] = 0x7D;
  to_screen[G_H] = 0x40;
  to_screen[G_UP] = 0x1E;
  to_screen[G_DN] = 0x16;                /* 'v': no down arrow in this set */
  to_screen[G_LT] = 0x1F;
  to_screen[G_RT] = 0x3E;                /* '>': no right arrow in this set */
  to_screen[G_PEAK_L] = 0x20;            /* the Atari's peak has no glyph in this set */
  to_screen[G_PEAK_R] = 0x20;
}

void scr_init(void)
{
  uint8_t r;

  build_table();
  VIC_BORDER = COLOR_BLUE;
  VIC_BACKGROUND = COLOR_BLUE;
  VIC_MEMORY = 0x17;                     /* lower/upper case: URIs are case sensitive */
  KERNAL_SHIFT_LOCK = 0x80;              /* and C=+SHIFT must not change it */
  memset(SCREEN, 0x20, 1000);
  memset(COLORS, COLOR_LIGHT_GREY, 1000);
  for (r = 0; r < SCR_ROWS; r++) {
    row_invert[r] = 0;
    scr_theme(r, T_BODY);
  }
}

void scr_coldstart(void)
{
  /* KERNAL and BASIC back in, then the reset vector: what the reset line does.
   * The FujiNet keeps its mounts, so the C64 restarts with them on its drives. */
  PROCESSOR_PORT = 0x37;
  __asm__("sei");
  __asm__("jmp ($FFFC)");
}

void scr_theme(uint8_t row, uint8_t theme)
{
  uint8_t i;
  uint8_t invert;
  uint8_t *p;

  invert = theme_invert[theme];
  if (invert != row_invert[row]) {
    p = cell(row, 0);
    for (i = 0; i < SCR_COLS; i++)
      p[i] ^= INVERT;
    row_invert[row] = invert;
  }
  memset(COLORS + (unsigned) row * SCR_COLS, theme_color[theme], SCR_COLS);
}

void scr_copy_row(uint8_t to, uint8_t from)
{
  uint8_t i;
  uint8_t flip;
  uint8_t *dst;
  const uint8_t *src;

  dst = cell(to, 0);
  src = cell(from, 0);
  flip = (uint8_t) (row_invert[to] ^ row_invert[from]);
  for (i = 0; i < SCR_COLS; i++)
    dst[i] = (uint8_t) (src[i] ^ flip);
}

static void draw(uint8_t row, uint8_t col, const char *s, uint8_t w, uint8_t invert)
{
  uint8_t *p;
  uint8_t c;
  uint8_t i;

  p = cell(row, col);
  invert ^= row_invert[row];
  for (i = 0; i < w; i++) {
    c = (uint8_t) *s;
    if (c) {
      ++s;
      p[i] = (uint8_t) ((to_screen[c & 0x7F] | (c & INVERT)) ^ invert);
    } else {
      p[i] = (uint8_t) (0x20 ^ invert);
    }
  }
}

void scr_text(uint8_t row, uint8_t col, const char *s, uint8_t w)
{
  draw(row, col, s, w, 0);
}

void scr_text_inv(uint8_t row, uint8_t col, const char *s, uint8_t w)
{
  draw(row, col, s, w, INVERT);
}

void scr_fill(uint8_t row, uint8_t col, uint8_t len, char ch)
{
  uint8_t code;

  code = (uint8_t) ((to_screen[(uint8_t) ch & 0x7F] | ((uint8_t) ch & INVERT)) ^ row_invert[row]);
  memset(cell(row, col), code, len);
}

char *scr_dec16_at(char *p, unsigned v, uint8_t w)
{
  char digits[5];
  uint8_t n;

  n = 0;
  do {
    digits[n++] = (char) ('0' + (uint8_t) (v % 10));
    v /= 10;
  } while (v);
  while (w > n) {
    *p++ = '0';
    --w;
  }
  while (n)
    *p++ = digits[--n];
  return p;
}

void scr_uint(uint8_t row, uint8_t col, unsigned v, uint8_t w)
{
  char text[6];
  char *end;
  uint8_t n;

  end = scr_dec16_at(text, v, 0);
  *end = 0;
  n = (uint8_t) (end - text);
  if (n < w) {
    scr_fill(row, col, (uint8_t) (w - n), ' ');
    scr_text(row, (uint8_t) (col + w - n), text, n);
  } else {
    scr_text(row, col, text + (n - w), w);
  }
}

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
  thumb_len = (uint8_t) ((uint16_t) (track * visible) / total);
  if (thumb_len < 1)
    thumb_len = 1;
  thumb_pos = (uint8_t) ((uint32_t) (track - thumb_len) * top / (total - visible));
  for (r = 0; r < thumb_len; r++)
    scr_putc((uint8_t) (first_row + 1 + thumb_pos + r), SCR_COLS - 1, (char) (INVERT | ' '));
}

void help_rows(uint8_t row, const char *p)
{
  uint8_t n;

  while (*p) {
    n = 0;
    while (p[n] && p[n] != '|')
      ++n;
    /* scr_text stops at a NUL only, so copy the part this row shows */
    {
      uint8_t i;
      uint8_t shown;
      uint8_t *dst;
      uint8_t invert;

      shown = n < 36 ? n : 36;
      dst = cell(row, 2);
      invert = row_invert[row];
      for (i = 0; i < shown; i++)
        dst[i] = (uint8_t) (to_screen[(uint8_t) p[i] & 0x7F] ^ invert);
    }
    p += n;
    if (*p == '|')
      ++p;
    ++row;
  }
}
