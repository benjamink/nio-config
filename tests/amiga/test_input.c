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
}
