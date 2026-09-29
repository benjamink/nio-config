#include "check.h"
#include "amiga_list.h"

void test_list(void)
{
  amiga_list_t l;
  uint16_t pot, body, idx;

  amiga_list_init(&l, 10);
  CHECK(l.selected == AMIGA_LIST_NONE && l.count == 0 && l.top == 0);
  amiga_list_move(&l, 1);
  CHECK(l.selected == AMIGA_LIST_NONE);
  CHECK(!amiga_list_hit(&l, 0, 9, &idx));
  amiga_list_prop(&l, &pot, &body);
  CHECK(pot == 0 && body == 0xFFFF);

  amiga_list_set_count(&l, 5);
  CHECK(l.selected == 0 && l.top == 0);

  amiga_list_set_count(&l, 100);
  amiga_list_move(&l, 15);
  CHECK(l.selected == 15 && l.top == 6);
  amiga_list_move(&l, -100);
  CHECK(l.selected == 0 && l.top == 0);
  amiga_list_select(&l, 99);
  CHECK(l.selected == 99 && l.top == 90);
  amiga_list_select(&l, 500);
  CHECK(l.selected == 99);
  amiga_list_prop(&l, &pot, &body);
  CHECK(body == 6553 && pot == 0xFFFF);

  /* Dragging the scroller moves the view, not the selection. */
  amiga_list_set_top_from_pot(&l, 0x7FFF);
  CHECK(l.top == 45 && l.selected == 99);

  CHECK(amiga_list_hit(&l, 0, 9, &idx) && idx == 45);
  CHECK(amiga_list_hit(&l, 89, 9, &idx) && idx == 54);
  CHECK(!amiga_list_hit(&l, 90, 9, &idx));
  CHECK(!amiga_list_hit(&l, -1, 9, &idx));

  amiga_list_select(&l, 99);
  amiga_list_set_count(&l, 50);
  CHECK(l.selected == 49 && l.top == 40);

  /* A hit below the last entry of a short list selects nothing. */
  amiga_list_set_count(&l, 3);
  CHECK(!amiga_list_hit(&l, 27, 9, &idx));

  amiga_list_set_rows(&l, 0);
  CHECK(l.rows == 1);

  amiga_list_set_count(&l, 0);
  CHECK(l.selected == AMIGA_LIST_NONE && l.top == 0);
}
