#include "trs80_console.h"

extern uint8_t con_r, con_c, con_ch;
extern void con_putc_raw(void);
extern void con_cursor_raw(void);
extern int con_key_raw(void);

void con_fill(uint8_t row, uint8_t col, uint8_t n, char c)
{
    con_r = row;
    con_ch = (uint8_t) c;
    while (n--) {
        con_c = col++;
        con_putc_raw();
    }
}

void con_put(uint8_t row, uint8_t col, const char *s)
{
    con_r = row;
    while (*s && col < CON_COLS) {
        con_c = col++;
        con_ch = (uint8_t) *s++;
        con_putc_raw();
    }
}

void con_clear(void)
{
    uint8_t r;
    for (r = 0; r < CON_ROWS; r++)
        con_fill(r, 0, CON_COLS, ' ');
    con_cursor(0, 0);
}

void con_cursor(uint8_t row, uint8_t col)
{
    con_r = row;
    con_c = col;
    con_cursor_raw();
}

int con_getkey(void)
{
    return con_key_raw();
}
