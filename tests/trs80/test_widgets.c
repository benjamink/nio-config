#include "trs80_console.h"
#include "trs80_keys.h"
#include "ui_widgets.h"
#include <stdio.h>
#include <string.h>

extern char scr[CON_ROWS][CON_COLS + 1];
void fake_reset(const int *k, int n);

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static const char *item(uint16_t i, void *ctx)
{
  static char b[16];
  (void) ctx;
  sprintf(b, "item %u", i);
  return b;
}

static void test_field_edit_max_length(void)
{
  int keys[300];
  char buf[256] = "";
  for (int i = 0; i < 300; i++) keys[i] = 'a';
  keys[299] = KEY_ENTER;
  fake_reset(keys, 300);
  CHECK(field_edit(buf, 255, 5, 0, 60) == 1);
  CHECK(strlen(buf) == 255);
  CHECK(buf[255] == 0);
}

static void test_field_edit_break_cancels(void)
{
  int keys[] = {'x', 'y', KEY_BREAK};
  char buf[16] = "abc";
  fake_reset(keys, 3);
  CHECK(field_edit(buf, 15, 5, 0, 20) == 0);
  CHECK(strcmp(buf, "abc") == 0);
}

static void test_field_edit_backspace_and_clear(void)
{
  int keys[] = {'d', KEY_LEFT, KEY_LEFT, 'z', KEY_ENTER};
  int keys2[] = {KEY_SLEFT, 'q', KEY_ENTER};
  char buf[16] = "abc";
  fake_reset(keys, 5);
  CHECK(field_edit(buf, 15, 5, 0, 20) == 1);
  CHECK(strcmp(buf, "abz") == 0);
  fake_reset(keys2, 3);
  CHECK(field_edit(buf, 15, 5, 0, 20) == 1);
  CHECK(strcmp(buf, "q") == 0);
}

static void test_field_edit_window_shows_the_end(void)
{
  int keys[] = {KEY_ENTER};
  char buf[32] = "0123456789ABCDEFGHIJ";
  fake_reset(keys, 1);
  field_edit(buf, 31, 5, 2, 8);
  CHECK(memcmp(&scr[5][2], "DEFGHIJ", 7) == 0);   /* last 7 chars + cursor slot */
}

static void test_list_pages_with_shift_down(void)
{
  list_t l = {2, 0, 40, 20, 50, 0, 0, item, 0};
  fake_reset(0, 0);
  CHECK(list_key(&l, KEY_SDOWN) == LIST_MOVED);
  CHECK(l.sel == 19);
  CHECK(l.top <= l.sel && l.sel < l.top + 20);
  list_key(&l, KEY_SDOWN); list_key(&l, KEY_SDOWN);
  CHECK(l.sel == 49);                             /* clamped */
  list_key(&l, KEY_SUP);
  CHECK(l.sel == 30);
}

static void test_list_edge_flags_and_marker(void)
{
  list_t l = {2, 0, 40, 5, 3, 0, 0, item, 0};
  fake_reset(0, 0);
  CHECK(list_key(&l, KEY_UP) == LIST_EDGE_UP);
  CHECK(list_key(&l, KEY_DOWN) == LIST_MOVED);
  CHECK(list_key(&l, KEY_DOWN) == LIST_MOVED);
  CHECK(list_key(&l, KEY_DOWN) == LIST_EDGE_DOWN);
  CHECK(l.sel == 2);
  CHECK(list_key(&l, 'x') == LIST_NONE);
  list_draw(&l);
  CHECK(scr[2][0] == ' ' && scr[4][0] == '>');
  CHECK(memcmp(&scr[4][2], "item 2", 6) == 0);
}

static void test_list_empty(void)
{
  list_t l = {2, 0, 40, 5, 0, 0, 0, item, 0};
  fake_reset(0, 0);
  CHECK(list_key(&l, KEY_DOWN) == LIST_EDGE_DOWN);
  CHECK(list_key(&l, KEY_SDOWN) == LIST_EDGE_DOWN);
  list_draw(&l);                                  /* must not read item 0 */
}

static void test_list_key_draw_repaints_only_what_moved(void)
{
  list_t l = {2, 0, 40, 5, 9, 0, 0, item, 0};
  fake_reset(0, 0);
  list_draw(&l);
  scr[5][0] = '#';                                /* a sentinel on a row a plain move must leave alone */
  CHECK(list_key_draw(&l, KEY_DOWN) == LIST_MOVED);
  CHECK(scr[2][0] == ' ' && scr[3][0] == '>');    /* old and new rows repainted */
  CHECK(scr[5][0] == '#');                        /* untouched */
  CHECK(list_key_draw(&l, 'x') == LIST_NONE);
  l.sel = 4;
  fake_reset(0, 0);
  list_draw(&l);
  CHECK(list_key_draw(&l, KEY_DOWN) == LIST_MOVED);   /* scrolls: the whole window repaints */
  CHECK(l.top == 1 && scr[2][0] == ' ' && memcmp(&scr[2][2], "item 1", 6) == 0 && scr[6][0] == '>');
}

int main(void)
{
  test_list_key_draw_repaints_only_what_moved();
  test_field_edit_max_length();
  test_field_edit_break_cancels();
  test_field_edit_backspace_and_clear();
  test_field_edit_window_shows_the_end();
  test_list_pages_with_shift_down();
  test_list_edge_flags_and_marker();
  test_list_empty();
  printf(failures ? "%d FAILED\n" : "all widget tests passed\n", failures);
  return failures != 0;
}
