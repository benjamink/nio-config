#ifndef TRS80_CONSOLE_H
#define TRS80_CONSOLE_H

#include <stdint.h>

#define CON_ROWS 24
#define CON_COLS 80

/* 80x24 text console. On the 4P these go through the LS-DOS @VDCTL and @KEY SVCs
 * (trs80_console.c); the host tests supply a fake with the same functions. */
void con_clear(void);
void con_put(uint8_t row, uint8_t col, const char *s);
void con_fill(uint8_t row, uint8_t col, uint8_t n, char c);
void con_cursor(uint8_t row, uint8_t col);
int con_getkey(void);

/* LS-DOS services beyond the screen (trs80_console.c; not part of the host-test fake). */
uint8_t dos_drive_enabled(uint8_t unit);   /* 0 while the drive's DCT is still the disabled stub nioboot leaves */
void dos_exec(const char *cmd);            /* leave CONFIG and run a CR-terminated LS-DOS command; does not return */

#endif
