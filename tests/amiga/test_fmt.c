#include "check.h"
#include "amiga_fmt.h"

/* amiga_sprintf replaces the C library's sprintf in the Amiga code: the
 * WB1.3 (nix13) build formats through Kickstart 1.3's RawDoFmt, which has
 * no %u and a 16-bit %d, so every number printed as "u" there. */
void test_fmt(void)
{
  char buf[128];

  CHECK(amiga_sprintf(buf, "%u", 0u) == 1);
  CHECK_STR(buf, "0");
  amiga_sprintf(buf, "%u of %u", 5u, 8u);
  CHECK_STR(buf, "5 of 8");
  amiga_sprintf(buf, "%u", 70000u);            /* > 16 bits */
  CHECK_STR(buf, "70000");
  amiga_sprintf(buf, "%d/%d", -12, 34);
  CHECK_STR(buf, "-12/34");
  amiga_sprintf(buf, "%lu", 4294967295UL);
  CHECK_STR(buf, "4294967295");
  amiga_sprintf(buf, "%2u|%3u|%02u|%02u", 7u, 42u, 5u, 123u);
  CHECK_STR(buf, " 7| 42|05|123");
  amiga_sprintf(buf, "%-5s|%8s|%13s", "DN0:", "date", "Drawer");
  CHECK_STR(buf, "DN0: |    date|       Drawer");
  amiga_sprintf(buf, "%.5s|%.40s", "abcdefgh", "ab");
  CHECK_STR(buf, "abcde|ab");
  amiga_sprintf(buf, "%c%c 100%%", 'O', 'K');
  CHECK_STR(buf, "OK 100%");
  amiga_sprintf(buf, "%s", (const char *) 0);
  CHECK_STR(buf, "(null)");
  CHECK(amiga_sprintf(buf, "Slot %u saved", 12u) == 13);
  CHECK_STR(buf, "Slot 12 saved");
}
