/* A recording 80x24 console for the host tests. */
#include "trs80_console.h"
#include <string.h>

char scr[CON_ROWS][CON_COLS + 1];
static const int *keys;
static int nkeys, kpos;

void fake_reset(const int *k, int n)
{
  memset(scr, ' ', sizeof scr);
  for (int r = 0; r < CON_ROWS; r++) scr[r][CON_COLS] = 0;
  keys = k; nkeys = n; kpos = 0;
}
void con_clear(void) { fake_reset(keys, nkeys); }
void con_put(uint8_t row, uint8_t col, const char *s)
{
  while (*s && col < CON_COLS) scr[row][col++] = *s++;
}
void con_fill(uint8_t row, uint8_t col, uint8_t n, char c)
{
  while (n-- && col < CON_COLS) scr[row][col++] = c;
}
void con_cursor(uint8_t row, uint8_t col) { (void) row; (void) col; }
int con_getkey(void) { return kpos < nkeys ? keys[kpos++] : -1; }
