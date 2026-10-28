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
void scr_clear(void);
void scr_theme(uint8_t row, uint8_t theme);
void scr_fill(uint8_t row, uint8_t col, uint8_t len, char ch);
void scr_putc(uint8_t row, uint8_t col, char ch);
/* Draw s in `width` cells (truncated, padded with blanks). */
void scr_text(uint8_t row, uint8_t col, const char *s, uint8_t width);
void scr_text_inv(uint8_t row, uint8_t col, const char *s, uint8_t width);
void scr_uint(uint8_t row, uint8_t col, unsigned long v, uint8_t width);
void scr_scrollbar(uint8_t first_row, uint8_t rows, uint16_t top,
                   uint16_t visible, uint16_t total);

/* Defined in atari_scr.s */
void scr_vectors_set(void);
void scr_vectors_off(void);
void scr_sio_hook(void);   /* after fn_init(): keep DLIs off during SIO */

extern uint8_t row_bg[SCR_ROWS];
extern uint8_t row_fg[SCR_ROWS];

#endif
