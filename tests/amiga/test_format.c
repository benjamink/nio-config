#include "check.h"
#include "config_nio.h"
#include "amiga_format.h"

void test_format(void)
{
  char buf[64];

  amiga_format_size(buf, 0, CONFIG_NIO_PREF_SIZE_FULL);
  CHECK_STR(buf, "0");
  amiga_format_size(buf, 999, CONFIG_NIO_PREF_SIZE_FULL);
  CHECK_STR(buf, "999");
  amiga_format_size(buf, 901120, CONFIG_NIO_PREF_SIZE_FULL);
  CHECK_STR(buf, "901,120");
  amiga_format_size(buf, 4294967295UL, CONFIG_NIO_PREF_SIZE_FULL);
  CHECK_STR(buf, "4,294,967,295");

  amiga_format_size(buf, 999, CONFIG_NIO_PREF_SIZE_COMPACT);
  CHECK_STR(buf, "999");
  amiga_format_size(buf, 1000, CONFIG_NIO_PREF_SIZE_COMPACT);
  CHECK_STR(buf, "1Kb");
  amiga_format_size(buf, 1536, CONFIG_NIO_PREF_SIZE_COMPACT);
  CHECK_STR(buf, "1.5Kb");
  amiga_format_size(buf, 123456, CONFIG_NIO_PREF_SIZE_COMPACT);
  CHECK_STR(buf, "120Kb");
  amiga_format_size(buf, 1048576, CONFIG_NIO_PREF_SIZE_COMPACT);
  CHECK_STR(buf, "1Mb");
  amiga_format_size(buf, 4294967295UL, CONFIG_NIO_PREF_SIZE_COMPACT);
  CHECK_STR(buf, "4Gb");

  CHECK(!amiga_format_date(buf, 0, CONFIG_NIO_PREF_DATE_YMD));
  CHECK_STR(buf, "?\?-?\?-?\?");
  CHECK(amiga_format_date(buf, 951782400UL, CONFIG_NIO_PREF_DATE_YMD));
  CHECK_STR(buf, "00-02-29");
  CHECK(amiga_format_date(buf, 951782400UL, CONFIG_NIO_PREF_DATE_YDM));
  CHECK_STR(buf, "00-29-02");
  CHECK(amiga_format_date(buf, 1709164800UL, CONFIG_NIO_PREF_DATE_YMD));
  CHECK_STR(buf, "24-02-29");
  CHECK(amiga_format_date(buf, 1735689600UL, CONFIG_NIO_PREF_DATE_YMD));
  CHECK_STR(buf, "25-01-01");
  CHECK(amiga_format_date(buf, 4102444799UL, CONFIG_NIO_PREF_DATE_YMD));
  CHECK_STR(buf, "99-12-31");

  amiga_clip_head(buf, sizeof(buf), "short.adf", 20);
  CHECK_STR(buf, "short.adf");
  amiga_clip_head(buf, sizeof(buf), "a_very_long_disk_name.adf", 12);
  CHECK_STR(buf, "a_very_lo...");
  amiga_clip_tail(buf, sizeof(buf), "tnfs://host/games/amiga/demos/", 16);
  CHECK_STR(buf, ".../amiga/demos/");   /* "..." + last 13 characters */
  amiga_clip_head(buf, sizeof(buf), "abcdef", 3);
  CHECK_STR(buf, "abc");
  amiga_clip_head(buf, 4, "abcdefgh", 20);  /* the buffer bounds the output */
  CHECK_STR(buf, "abc");
  amiga_clip_head(buf, sizeof(buf), NULL, 10);
  CHECK_STR(buf, "");

  amiga_last_line(buf, sizeof(buf),
                  "FMOUNT INSPECT=ok\nCannot open fujinet-disk.device\n\n");
  CHECK_STR(buf, "Cannot open fujinet-disk.device");
  amiga_last_line(buf, sizeof(buf), "one line, no newline");
  CHECK_STR(buf, "one line, no newline");
  amiga_last_line(buf, sizeof(buf), "\r\n  \n");
  CHECK_STR(buf, "");
  amiga_last_line(buf, 6, "abcdefghij\n");
  CHECK_STR(buf, "abcde");

  /* The Shell appends "<cmd> failed returncode N" after the command's own
   * message; the command's message is the useful one. */
  amiga_last_line(buf, sizeof(buf),
                  "FMOUNT INSPECT=ok\nUnsupported candidate media\n"
                  "SYS:C/fmount failed returncode 10\n");
  CHECK_STR(buf, "Unsupported candidate media");
  amiga_last_line(buf, sizeof(buf), "fmount: Unknown command\n"
                  "fmount failed returncode 10\n");
  CHECK_STR(buf, "fmount: Unknown command");
  amiga_last_line(buf, sizeof(buf), "SYS:C/fmount failed returncode 20\n");
  CHECK_STR(buf, "SYS:C/fmount failed returncode 20");
}
