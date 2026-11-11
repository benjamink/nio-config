#include "check.h"
#include "amiga_drives.h"
#include <string.h>

void test_drives(void)
{
  char buf[64];

  CHECK_STR(amiga_drive_label(0, 0), "DN0:");
  CHECK_STR(amiga_drive_label(7, 0), "DN7:");
  CHECK_STR(amiga_drive_label(2, 1), "HN0:");
  CHECK_STR(amiga_drive_label(4, 1), "DO0:");
  CHECK_STR(amiga_drive_label(7, 1), "HO1:");
  CHECK(amiga_drive_label(8, 0) == NULL);

  CHECK(amiga_drive_unit("dn3:", 0) == 3);
  CHECK(amiga_drive_unit("DN3", 0) == 3);
  CHECK(amiga_drive_unit("DN3:", 1) == -1);
  CHECK(amiga_drive_unit("ho0:", 1) == 6);
  CHECK(amiga_drive_unit("DN0::", 0) == -1);
  CHECK(amiga_drive_unit("", 0) == -1);
  CHECK(amiga_drive_unit(NULL, 0) == -1);

  CHECK(amiga_fmount_command(buf, sizeof(buf), "SYS:C/fmount", 12, 0, 0, 1));
  CHECK_STR(buf, "SYS:C/fmount 12 DN0: RO");
  CHECK(amiga_fmount_command(buf, sizeof(buf), "SYS:C/fmount", 255, 3, 1, 0));
  CHECK_STR(buf, "SYS:C/fmount 255 HN1: RW");
  CHECK(amiga_fumount_command(buf, sizeof(buf), "SYS:C/fumount", 5, 0));
  CHECK_STR(buf, "SYS:C/fumount 5");
  CHECK(amiga_fumount_command(buf, sizeof(buf), "SYS:C/fumount", 5, 1));
  CHECK_STR(buf, "SYS:C/fumount DO1:");

  /* A path with a space would be split by the command line parser. */
  CHECK(!amiga_fmount_command(buf, sizeof(buf), "Work:My Tools/fmount", 1, 0, 0, 0));
  CHECK(!amiga_fmount_command(buf, 10, "SYS:C/fmount", 1, 0, 0, 0));
  CHECK(!amiga_fmount_command(buf, sizeof(buf), "SYS:C/fmount", 1, 8, 0, 0));
  CHECK(!amiga_fumount_command(buf, sizeof(buf), "", 0, 0));

  /* Media classification and drive fit. */
  {
    static const uint8_t ffs[4] = { 'D', 'O', 'S', 1 };
    static const uint8_t ofs[4] = { 'D', 'O', 'S', 0 };
    static const uint8_t bad[4] = { 'N', 'D', 'O', 'S' };
    amiga_media_t dd, hd, hdo, none, pin;

    amiga_media_classify(&dd, 512, 1760, ffs, 4);
    amiga_media_classify(&hd, 512, 3520, ffs, 4);
    amiga_media_classify(&hdo, 512, 3520, ofs, 4);
    amiga_media_classify(&none, 512, 3520, bad, 4);
    CHECK(dd.known && !dd.hd && !dd.ofs);
    CHECK(hd.known && hd.hd && !hd.ofs);
    CHECK(hdo.known && hdo.hd && hdo.ofs);
    CHECK(!none.known);
    amiga_media_classify(&none, 256, 1760, ffs, 4);
    CHECK(!none.known);
    amiga_media_classify(&none, 512, 2000, ffs, 4);
    CHECK(!none.known);
    CHECK_STR(amiga_media_name(&hdo), "HD OFS");
    CHECK_STR(amiga_media_name(&dd), "DD FFS");
    CHECK_STR(amiga_media_name(&none), "?");

    /* WB2+: free units take anything; a pinned handler only its own type. */
    CHECK(amiga_drive_fit(0, 0, &hd, NULL) == AMIGA_FIT_OK);
    pin = dd;
    CHECK(amiga_drive_fit(0, 0, &hd, &pin) == AMIGA_FIT_DENSITY);
    CHECK(amiga_drive_fit(0, 0, &hdo, &hd) == AMIGA_FIT_FS);
    CHECK(amiga_drive_fit(0, 0, &dd, &pin) == AMIGA_FIT_OK);
    CHECK(amiga_drive_fit(0, 0, &none, &pin) == AMIGA_FIT_OK);

    /* WB1.3: the endpoint fixes density and filesystem. */
    CHECK(amiga_drive_fit(0, 1, &dd, NULL) == AMIGA_FIT_OK);    /* DN0 */
    CHECK(amiga_drive_fit(1, 1, &hd, NULL) == AMIGA_FIT_DENSITY);
    CHECK(amiga_drive_fit(2, 1, &hd, NULL) == AMIGA_FIT_OK);    /* HN0 */
    CHECK(amiga_drive_fit(3, 1, &hdo, NULL) == AMIGA_FIT_FS);
    CHECK(amiga_drive_fit(6, 1, &hdo, NULL) == AMIGA_FIT_OK);   /* HO0 */
    CHECK(amiga_drive_fit(4, 1, &dd, NULL) == AMIGA_FIT_FS);    /* DO0 */
    CHECK(amiga_drive_fit(4, 1, &none, NULL) == AMIGA_FIT_OK);
  }

  /* Standard and Workbench 1.3 FMOUNT builds must match this config-nio. */
  {
    char msg[200];
    static const char standard[] =
      "Usage: FMOUNT slot [DN0:|...|DN7:|HD0:|...|HD3:] [RO|RW]";
    static const char kick13[] =
      "Usage: FMOUNT slot 0|1|DN0:|DN1:|HN0:|HN1:|DO0:|DO1:|HO0:|HO1: [RO|RW]";

    CHECK(amiga_fmount_flavour(standard) == AMIGA_TOOLS_STANDARD);
    CHECK(amiga_fmount_flavour(kick13) == AMIGA_TOOLS_KICK13);
    CHECK(amiga_fmount_flavour("fmount: Unknown command") == AMIGA_TOOLS_UNKNOWN);
    CHECK(amiga_fmount_flavour("") == AMIGA_TOOLS_UNKNOWN);
    CHECK(amiga_fmount_flavour(NULL) == AMIGA_TOOLS_UNKNOWN);

    /* Only WB3.1's FFS 40.1 pins a started handler; 3.0, 3.1.4 and 3.2 retire it. */
    CHECK(amiga_handler_pins_media(40));
    CHECK(!amiga_handler_pins_media(36));
    CHECK(!amiga_handler_pins_media(39));
    CHECK(!amiga_handler_pins_media(46));
    CHECK(!amiga_handler_pins_media(47));
    CHECK(!amiga_tools_mismatch(0, AMIGA_TOOLS_STANDARD, msg, sizeof(msg)));
    CHECK(!amiga_tools_mismatch(1, AMIGA_TOOLS_KICK13, msg, sizeof(msg)));
    CHECK(!amiga_tools_mismatch(0, AMIGA_TOOLS_UNKNOWN, msg, sizeof(msg)));
    CHECK(!amiga_tools_mismatch(1, AMIGA_TOOLS_UNKNOWN, msg, sizeof(msg)));
    CHECK(amiga_tools_mismatch(0, AMIGA_TOOLS_KICK13, msg, sizeof(msg)));
    CHECK(strstr(msg, "DN0: to DN7:") && strstr(msg, "WB204"));
    CHECK(amiga_tools_mismatch(1, AMIGA_TOOLS_STANDARD, msg, sizeof(msg)));
    CHECK(strstr(msg, "This is Workbench 1.3") && strstr(msg, "WB13"));
    CHECK(amiga_tools_mismatch(1, AMIGA_TOOLS_STANDARD, msg, 20));
    CHECK(strlen(msg) == 19);
  }
}
