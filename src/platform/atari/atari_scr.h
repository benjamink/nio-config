#ifndef ATARI_SCR_H
#define ATARI_SCR_H

#include <stdint.h>

#define SCR_COLS 40
#define SCR_ROWS 24

/* Colour schemes, one per screen row. */
enum {
  T_TITLE, T_TABS, T_FIELD, T_BODY, T_SEL, T_INFO, T_HINT, T_STATUS,
  T_ERROR, T_DIALOG, T_EDIT, T_COUNT
};

/* Text is ASCII.  Bytes 1..31 are raw ROM-charset glyphs (box drawing and
 * arrows); setting bit 7 inverts a character. */
#define G_TL 0x11
#define G_TR 0x05
#define G_BL 0x1A
#define G_BR 0x03
#define G_H  0x12
#define G_UP 0x1C
#define G_DN 0x1D
#define G_LT 0x1E
#define G_RT 0x1F
#define G_PEAK_L 0x08          /* with G_PEAK_R, the FujiNet peak */
#define G_PEAK_R 0x0A

void scr_init(void);
void scr_exit(void);
void scr_coldstart(void);
void scr_theme_run(void);
#define scr_theme(row, theme) do { scr_zr = (row); scr_zch = (theme); scr_theme_run(); } while (0)
/* Show everything drawn since the last call (see atari_scr.c); input_key() does
 * this before it waits, so only code that blocks elsewhere needs to. */
void scr_present(void);

/* Text drawing is in assembly (atari_scr_fast.s) with its arguments passed
 * through zero page: the macros store them and call the routine, which is far
 * cheaper on a 6502 than C's argument stack.  Arguments are evaluated once, in
 * order, and must not themselves draw. */
extern uint8_t scr_zr, scr_zc, scr_zw, scr_zinv, scr_zch;
extern const char *scr_zs;
extern unsigned scr_zv;
#pragma zpsym("scr_zr")
#pragma zpsym("scr_zc")
#pragma zpsym("scr_zw")
#pragma zpsym("scr_zinv")
#pragma zpsym("scr_zch")
#pragma zpsym("scr_zs")
#pragma zpsym("scr_zv")
void scr_text_run(void);
void scr_fill_run(void);
void scr_u16_run(void);
void scr_dec16_run(void);
extern uint8_t scr_rowlo[SCR_ROWS];
extern uint8_t scr_rowhi[SCR_ROWS];

/* Draw s in `w` cells (truncated, blank padded); _inv draws it inverse. */
#define scr_text(r, c, s, w) \
  do { scr_zr = (r); scr_zc = (c); scr_zs = (s); scr_zw = (w); scr_zinv = 0; \
       scr_text_run(); } while (0)
#define scr_text_inv(r, c, s, w) \
  do { scr_zr = (r); scr_zc = (c); scr_zs = (s); scr_zw = (w); scr_zinv = 0x80; \
       scr_text_run(); } while (0)
/* len cells of character ch */
#define scr_fill(r, c, len, ch) \
  do { scr_zr = (r); scr_zc = (c); scr_zw = (len); scr_zch = (ch); \
       scr_fill_run(); } while (0)
#define scr_putc(r, c, ch) scr_fill(r, c, 1, ch)
/* a 16-bit number, right aligned in w cells */
#define scr_uint(r, c, v, w) \
  do { scr_zr = (r); scr_zc = (c); scr_zv = (v); scr_zw = (w); \
       scr_u16_run(); } while (0)
void scr_scrollbar(uint8_t first_row, uint8_t rows, uint16_t top,
                   uint16_t visible, uint16_t total);

/* Defined in atari_scr.s */
void scr_vectors_set(void);
void scr_vectors_off(void);
void scr_sio_hook(void);   /* after fn_init(): keep DLIs off during SIO */
void scr_flat_on(void);    /* keep DLIs off across SIO, retain vertical blank */
void scr_flat_off(void);   /* restore normal row colours after a failed mount */

extern uint8_t row_bg[SCR_ROWS];
extern uint8_t row_fg[SCR_ROWS];

/* v as ASCII digits at p (at least `w` of them, zero padded), advancing p. */
#define scr_dec16(p, v, w) \
  do { scr_zs = (p); scr_zv = (v); scr_zw = (w); scr_dec16_run(); \
       (p) = (char *) scr_zs; } while (0)

#endif
