#include "check.h"
#include "amiga_launch.h"

#include <string.h>

/* A fake volume: path -> entries. */
typedef struct {
  const char *path;
  amiga_launch_entry_t e[12];
  uint16_t n;
} fake_dir_t;

static const fake_dir_t FAKE[] = {
  { "DN0:", {
      { "ReadMe", 0, 0, 1390 },
      { "KoiPondECS", 0, 1, 224036 },
      { "Source", 1, 0, 0 },
      { "KoiPond", 0, 1, 177252 },
      { "S", 1, 0, 0 },
      { "Disk Tool", 0, 1, 9000 },
      { "LICENSE", 0, 0, 35149 } }, 7 },
  { "DN0:Source/", { { "src", 1, 0, 0 }, { "build", 0, 1, 100 } }, 2 },
  { "DN0:Source/src/", { { "notes", 0, 0, 5 } }, 1 },
  { "DN1:", { { "ReadMe", 0, 0, 10 } }, 1 },
};

static int fake_scan(const char *path, amiga_launch_entry_t *out, uint16_t max,
                     uint16_t *count, void *ctx)
{
  size_t i;

  (void) ctx;
  for (i = 0; i < sizeof(FAKE) / sizeof(FAKE[0]); i++) {
    if (strcmp(FAKE[i].path, path) == 0) {
      uint16_t n = FAKE[i].n > max ? max : FAKE[i].n;

      memcpy(out, FAKE[i].e, n * sizeof(out[0]));
      *count = n;
      return 1;
    }
  }
  return 0;   /* unreadable */
}

void test_launch(void)
{
  static amiga_launch_t l;
  char cmd[AMIGA_LAUNCH_CMD_MAX];

  amiga_launch_init(&l, 8, fake_scan, NULL);

  /* Root: drawers first, then programs; plain files are hidden. */
  CHECK(amiga_launch_open(&l, "DN0:"));
  CHECK(l.depth == 0 && amiga_launch_rows(&l) == 5);
  CHECK(!amiga_launch_row_is_parent(&l, 0));
  CHECK_STR(amiga_launch_row_entry(&l, 0)->name, "S");
  CHECK_STR(amiga_launch_row_entry(&l, 1)->name, "Source");
  CHECK_STR(amiga_launch_row_entry(&l, 2)->name, "Disk Tool");
  CHECK_STR(amiga_launch_row_entry(&l, 3)->name, "KoiPond");
  CHECK_STR(amiga_launch_row_entry(&l, 4)->name, "KoiPondECS");
  CHECK(amiga_launch_row_entry(&l, 5) == NULL);
  /* The cursor starts on the first program, not on a drawer. */
  CHECK(l.list.selected == 2);

  /* Commands: quoted when the path has a space, arguments appended. */
  CHECK(amiga_launch_command(&l, 4, "", cmd, sizeof(cmd)));
  CHECK_STR(cmd, "DN0:KoiPondECS");
  CHECK(amiga_launch_command(&l, 4, "  AUTO  ", cmd, sizeof(cmd)));
  CHECK_STR(cmd, "DN0:KoiPondECS AUTO");
  CHECK(amiga_launch_command(&l, 2, "-x", cmd, sizeof(cmd)));
  CHECK_STR(cmd, "\"DN0:Disk Tool\" -x");
  CHECK(!amiga_launch_command(&l, 1, "", cmd, sizeof(cmd)));   /* a drawer */
  CHECK(!amiga_launch_command(&l, 9, "", cmd, sizeof(cmd)));   /* no row */
  CHECK(!amiga_launch_command(&l, 4, "AUTO", cmd, 14));         /* too small */
  CHECK(amiga_launch_command(&l, 4, NULL, cmd, sizeof(cmd)));
  CHECK_STR(cmd, "DN0:KoiPondECS");

  /* Programs report PROGRAM and do not move. */
  CHECK(amiga_launch_activate(&l, 4) == AMIGA_LAUNCH_PROGRAM);
  CHECK_STR(l.path, "DN0:");

  /* Into a drawer: ".." heads the rows and the command path follows. */
  CHECK(amiga_launch_activate(&l, 1) == AMIGA_LAUNCH_ENTERED);
  CHECK_STR(l.path, "DN0:Source/");
  CHECK(l.depth == 1 && amiga_launch_rows(&l) == 3);
  CHECK(amiga_launch_row_is_parent(&l, 0));
  CHECK(amiga_launch_row_entry(&l, 0) == NULL);
  CHECK_STR(amiga_launch_row_entry(&l, 1)->name, "src");
  CHECK_STR(amiga_launch_row_entry(&l, 2)->name, "build");
  CHECK(l.list.selected == 2);   /* the program, past ".." and the drawer */
  CHECK(amiga_launch_command(&l, 2, "", cmd, sizeof(cmd)));
  CHECK_STR(cmd, "DN0:Source/build");

  /* Two deep, then back up one level at a time. */
  CHECK(amiga_launch_activate(&l, 1) == AMIGA_LAUNCH_ENTERED);
  CHECK_STR(l.path, "DN0:Source/src/");
  CHECK(l.depth == 2 && amiga_launch_rows(&l) == 1);   /* only ".." */
  CHECK(amiga_launch_activate(&l, 0) == AMIGA_LAUNCH_ENTERED);
  CHECK_STR(l.path, "DN0:Source/");
  CHECK(amiga_launch_parent(&l));
  CHECK_STR(l.path, "DN0:");
  CHECK(l.depth == 0);
  CHECK(!amiga_launch_parent(&l));   /* already at the root */

  /* A drive with nothing to run: no rows, and say so rather than fail. */
  CHECK(amiga_launch_open(&l, "DN1:"));
  CHECK(amiga_launch_rows(&l) == 0);
  CHECK(!amiga_launch_command(&l, 0, "", cmd, sizeof(cmd)));

  /* An unreadable drive fails with a reason and keeps the old listing. */
  CHECK(amiga_launch_open(&l, "DN0:"));
  CHECK(!amiga_launch_open(&l, "DN7:"));
  CHECK(l.error != NULL);
}
