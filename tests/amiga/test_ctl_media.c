#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"
#include "amiga_drives.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;
static amiga_media_t image;           /* what the fake inspector reports */
static int inspect_ok;
static amiga_media_t pinned[AMIGA_DRIVE_COUNT];
static char last_cmd[AMIGA_CMD_MAX];

static int inspect(const char *uri, amiga_media_t *media, void *ctx)
{
  (void) uri;
  (void) ctx;
  *media = image;
  return inspect_ok;
}

static int pin(uint8_t unit, amiga_media_t *out, void *ctx)
{
  (void) ctx;
  *out = pinned[unit];
  return pinned[unit].known;
}

static int fake_exec(const char *cmd, char *output, uint16_t cap, void *ctx)
{
  uint8_t map[17];
  unsigned slot;
  char label[8];
  char mode[4];
  int unit;

  (void) ctx;
  (void) cap;
  output[0] = 0;
  strcpy(last_cmd, cmd);
  memset(map, 0, sizeof(map));
  map[0] = 1;
  if (sscanf(cmd, "%*s %u %7s %3s", &slot, label, mode) == 3 &&
      (unit = amiga_drive_unit(label, ctl.kick13)) >= 0) {
    map[1 + unit * 2] = (uint8_t) (1 | (mode[1] == 'O' ? 2 : 0));
    map[2 + unit * 2] = (uint8_t) slot;
  }
  fake_appstore_put("config-nio", "mappings", map, sizeof(map));
  return 0;
}

static void setup(uint8_t kick13)
{
  fake_nio_reset();
  memset(pinned, 0, sizeof(pinned));
  fake_slot_put(3, "tnfs://x/Work.adf", 0);
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, kick13, 8, fake_exec, NULL);
  amiga_ctl_set_media_probes(&ctl, inspect, NULL, pin, NULL);
  last_cmd[0] = 0;
}

void test_ctl_media(void)
{
  char note[16];

  /* WB3.1: DN0/DN1 already run DD OFS handlers; an HD FFS image must be
   * suggested DN2 and refused on DN0. */
  setup(0);
  pinned[0].known = pinned[1].known = 1;
  pinned[0].ofs = pinned[1].ofs = 1;
  image.known = 1; image.hd = 1; image.ofs = 0;
  inspect_ok = 1;
  CHECK(amiga_ctl_mount_begin_slot(&ctl, 3));
  CHECK(ctl.drives.selected == 2);
  CHECK_STR(state.status, "Work.adf is HD FFS: suggested DN2:");
  amiga_ctl_drive_note(&ctl, 0, note, sizeof(note));
  CHECK_STR(note, "(DD OFS only)");
  amiga_ctl_drive_note(&ctl, 2, note, sizeof(note));
  CHECK_STR(note, "");
  CHECK(!amiga_ctl_mount_commit(&ctl, 0, 1));
  CHECK_STR(state.status, "Work.adf is HD FFS; DN0: cannot take it");
  CHECK_STR(last_cmd, "");              /* FMOUNT never ran */
  CHECK(amiga_ctl_mount_commit(&ctl, 2, 1));
  CHECK_STR(last_cmd, "fmount 3 DN2: RO");

  /* A DD OFS image fits the pinned drive and is suggested it. */
  setup(0);
  pinned[0].known = 1; pinned[0].ofs = 1;
  image.known = 1; image.hd = 0; image.ofs = 1;
  CHECK(amiga_ctl_mount_begin_slot(&ctl, 3));
  CHECK(ctl.drives.selected == 0);
  amiga_drive_label(0, 0);
  amiga_ctl_drive_note(&ctl, 0, note, sizeof(note));
  CHECK_STR(note, "[DD OFS]");

  /* Not inspectable: nothing excluded, first empty drive suggested. */
  setup(0);
  pinned[0].known = 1;
  inspect_ok = 0;
  CHECK(amiga_ctl_mount_begin_slot(&ctl, 3));
  CHECK(ctl.drives.selected == 0);
  CHECK_STR(state.status, "Choose a drive for Work.adf");
  CHECK(amiga_ctl_drive_fit(&ctl, 0) == AMIGA_FIT_OK);

  /* WB1.3: HO0: (unit 6) is the only home for HD OFS. */
  setup(1);
  inspect_ok = 1;
  image.known = 1; image.hd = 1; image.ofs = 1;
  CHECK(amiga_ctl_mount_begin_slot(&ctl, 3));
  CHECK(ctl.drives.selected == 6);
  CHECK_STR(state.status, "Work.adf is HD OFS: suggested HO0:");
  amiga_ctl_drive_note(&ctl, 0, note, sizeof(note));
  CHECK_STR(note, "(DD FFS only)");
  amiga_ctl_drive_note(&ctl, 6, note, sizeof(note));
  CHECK_STR(note, "[HD OFS]");
  CHECK(!amiga_ctl_mount_commit(&ctl, 2, 0));
  CHECK_STR(state.status, "Work.adf is HD OFS; HN0: cannot take it");

  /* No drive can take it: the message says so. */
  setup(0);
  pinned[0].known = 1;
  {
    uint8_t u;
    for (u = 1; u < AMIGA_DRIVE_COUNT; u++)
      pinned[u] = pinned[0];
  }
  image.known = 1; image.hd = 1; image.ofs = 0;
  CHECK(amiga_ctl_mount_begin_slot(&ctl, 3));
  CHECK_STR(state.status, "Work.adf is HD FFS: no drive can take it");
}
