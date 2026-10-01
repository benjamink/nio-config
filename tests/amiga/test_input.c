#include "check.h"
#include "amiga_input.h"

void test_input(void)
{
  CHECK(amiga_key_from_raw(0x4C, 0) == AMIGA_KEY_UP);
  CHECK(amiga_key_from_raw(0x4D, 0) == AMIGA_KEY_DOWN);
  CHECK(amiga_key_from_raw(0x4C, 0x0001) == AMIGA_KEY_PAGE_UP);
  CHECK(amiga_key_from_raw(0x4D, 0x0002) == AMIGA_KEY_PAGE_DOWN);
  CHECK(amiga_key_from_raw(0x4C, 0x0010) == AMIGA_KEY_TOP);
  CHECK(amiga_key_from_raw(0x4D, 0x0020) == AMIGA_KEY_BOTTOM);
  CHECK(amiga_key_from_raw(0x44, 0) == AMIGA_KEY_ACTIVATE);
  CHECK(amiga_key_from_raw(0x43, 0) == AMIGA_KEY_ACTIVATE);
  CHECK(amiga_key_from_raw(0x41, 0) == AMIGA_KEY_PARENT);
  CHECK(amiga_key_from_raw(0x45, 0) == AMIGA_KEY_CANCEL);
  CHECK(amiga_key_from_raw(0x5F, 0) == AMIGA_KEY_HELP);
  CHECK(amiga_key_from_raw(0x42, 0) == AMIGA_KEY_NEXT_PAGE);
  CHECK(amiga_key_from_raw(0x42, 0x0001) == AMIGA_KEY_PREV_PAGE);
  CHECK(amiga_key_from_raw(0x4C | 0x80, 0) == AMIGA_KEY_NONE);
  CHECK(amiga_key_from_raw(0x20, 0) == AMIGA_KEY_NONE);

  /* Digits on the main row and the keypad; key-up and others ignored. */
  CHECK(amiga_digit_from_raw(0x01) == 1);
  CHECK(amiga_digit_from_raw(0x09) == 9);
  CHECK(amiga_digit_from_raw(0x0A) == 0);
  CHECK(amiga_digit_from_raw(0x0F) == 0);
  CHECK(amiga_digit_from_raw(0x1D) == 1);
  CHECK(amiga_digit_from_raw(0x2E) == 5);
  CHECK(amiga_digit_from_raw(0x3F) == 9);
  CHECK(amiga_digit_from_raw(0x02 | 0x80) == -1);
  CHECK(amiga_digit_from_raw(0x20) == -1);
  CHECK(amiga_digit_from_raw(0x0B) == -1);

  /* Typing a number: digits within 2 s build it, a pause starts over,
   * and a digit that would pass the limit starts a new number. */
  {
    amiga_typeahead_t t;

    amiga_typeahead_reset(&t);
    CHECK(amiga_typeahead_feed(&t, 2, 1000, 255) == 2);
    CHECK(amiga_typeahead_feed(&t, 3, 2500, 255) == 23);
    CHECK(amiga_typeahead_feed(&t, 4, 4000, 255) == 234);
    CHECK(amiga_typeahead_feed(&t, 5, 5000, 255) == 5);    /* 2345 > 255 */
    CHECK(amiga_typeahead_feed(&t, 1, 7001, 255) == 1);    /* pause */
    CHECK(amiga_typeahead_feed(&t, 0, 7500, 255) == 10);
    CHECK(amiga_typeahead_feed(&t, 0, 8000, 255) == 100);
    CHECK(amiga_typeahead_feed(&t, 7, 8100, 255) == 7);    /* 1007 */
    CHECK(amiga_typeahead_feed(&t, 2, 8200, 255) == 72);
    CHECK(amiga_typeahead_feed(&t, 6, 8300, 255) == 6);    /* 726 */
    amiga_typeahead_reset(&t);
    CHECK(amiga_typeahead_feed(&t, 9, 8400, 255) == 9);
    /* The millisecond clock may wrap. */
    amiga_typeahead_reset(&t);
    CHECK(amiga_typeahead_feed(&t, 1, 0xFFFFFF00UL, 255) == 1);
    CHECK(amiga_typeahead_feed(&t, 2, 0x00000100UL, 255) == 12);
  }
}
