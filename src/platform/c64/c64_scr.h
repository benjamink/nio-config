#ifndef C64_SCR_H
#define C64_SCR_H

/* The screen the config app draws on, for the C64: the same calls as atari_scr.h
 * on the VIC-II's 40x25 text screen. The app uses the first 24 rows. */

#include <stdint.h>

#define SCR_COLS 40
#define SCR_ROWS 24

/* The jiffy clock's low byte. */
#define SCR_JIFFY (*(volatile uint8_t *) 0xA2)

/* The app's on-screen text that names this machine's keys. */
#define UI_HINT_HOSTS  "RET Browse A Add E Edit D Del F1 Boot"
#define UI_HINT_BROWSE "RET Open </> DEL Up F Filter H Help"
#define UI_HINT_DRIVES "RET Pick X Eject R Read-only F1 Boot"
#define UI_HINT_SETUP  "RET Join Wi-Fi R Refresh F1 Boot"
#define UI_INFO_DRIVES "F1 boots with these drives mounted"
#define UI_HELP_BROWSE "Browse|RET open; up/down move; </> page|SPACE/B page; DEL up; R reload|F filter (* ?; blank clears)"
#define UI_HELP_GLOBAL "F3/HOME change tab|F1 boot|F5 run the FujiNet demo|Joystick moves; fire RET|H or F7 opens this"

/* Colour schemes, one per screen row. */
enum {
  T_TITLE, T_TABS, T_FIELD, T_BODY, T_SEL, T_INFO, T_HINT, T_STATUS,
  T_ERROR, T_DIALOG, T_EDIT, T_COUNT
};

/* Text is ASCII. Bytes 1..31 are glyphs; setting bit 7 inverts a character. */
#define G_TL 0x11
#define G_TR 0x05
#define G_BL 0x1A
#define G_BR 0x03
#define G_H  0x12
#define G_UP 0x1C
#define G_DN 0x1D
#define G_LT 0x1E
#define G_RT 0x1F
#define G_PEAK_L 0x08
#define G_PEAK_R 0x0A

void scr_init(void);
void scr_coldstart(void);
/* Load and run the demo on the boot disk (c64_run.s). Does not return. */
void c64_run_demo(void);
void scr_theme(uint8_t row, uint8_t theme);
/* Copy a row's text to another row. Reverse video belongs to the row, not the text. */
void scr_copy_row(uint8_t to, uint8_t from);

/* Draw s in `w` cells (truncated, blank padded); _inv draws it inverse. */
void scr_text(uint8_t row, uint8_t col, const char *s, uint8_t w);
void scr_text_inv(uint8_t row, uint8_t col, const char *s, uint8_t w);
/* len cells of character ch */
void scr_fill(uint8_t row, uint8_t col, uint8_t len, char ch);
#define scr_putc(r, c, ch) scr_fill((r), (c), 1, (ch))
/* a 16-bit number, right aligned in w cells */
void scr_uint(uint8_t row, uint8_t col, unsigned v, uint8_t w);
void scr_scrollbar(uint8_t first_row, uint8_t rows, uint16_t top,
                   uint16_t visible, uint16_t total);
/* Rows of text separated by '|', starting at `row`, in column 2. */
void help_rows(uint8_t row, const char *p);

/* v as ASCII digits at p (at least `w` of them, zero padded), advancing p. */
char *scr_dec16_at(char *p, unsigned v, uint8_t w);
#define scr_dec16(p, v, w) do { (p) = scr_dec16_at((p), (v), (w)); } while (0)

/* The VIC-II shows screen memory directly and needs no interrupts for its colours,
 * so these hooks of the Atari's double-buffered display do nothing here. */
#define scr_present()
#define scr_sio_hook()
#define scr_reading 0
#define scr_reading_on()
#define scr_flat_on()
#define scr_flat_off()

#endif
