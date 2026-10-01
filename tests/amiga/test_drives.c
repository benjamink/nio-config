#include "check.h"
#include "amiga_drives.h"

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
}
