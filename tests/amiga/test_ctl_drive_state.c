#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"
#include "amiga_drives.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;
static uint8_t present[AMIGA_DRIVE_COUNT];   /* DOS drive exists (fake DosList) */
static char last_cmd[AMIGA_CMD_MAX];

static int probe(uint8_t unit, void *ctx)
{
  (void) ctx;
  return present[unit];
}

/* FMOUNT creates the DOS drive and records the mapping. */
static int fake_exec(const char *cmd, char *output, uint16_t cap, void *ctx)
{
  uint8_t map[17];
  const uint8_t *cur;
  uint16_t len;
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
  cur = fake_appstore_get("config-nio", "mappings", &len);
  if (cur && len == sizeof(map))
    memcpy(map, cur, sizeof(map));
  if (sscanf(cmd, "SYS:C/fmount %u %7s %3s", &slot, label, mode) == 3 &&
      (unit = amiga_drive_unit(label, 0)) >= 0) {
    map[1 + unit * 2] = (uint8_t) (1 | (mode[1] == 'O' ? 2 : 0));
    map[2 + unit * 2] = (uint8_t) slot;
    present[unit] = 1;
  }
  fake_appstore_put("config-nio", "mappings", map, sizeof(map));
  return 0;
}

void test_ctl_drive_state(void)
{
  /* After a reboot: DN0 and DN2 are saved on the FujiNet, only DN2 exists. */
  static const uint8_t saved[17] = { 1, 3, 1, 0, 0, 1, 7 };
  char name[8];

  fake_nio_reset();
  memset(present, 0, sizeof(present));
  fake_slot_put(1, "tnfs://x/Toolbox.adf", 1);
  fake_slot_put(7, "tnfs://x/Games.adf", 0);
  fake_appstore_put("config-nio", "mappings", saved, sizeof(saved));
  present[2] = 1;
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, fake_exec, NULL);

  /* Without a probe (e.g. WB1.3 static drives) the mapping is the truth. */
  CHECK(amiga_ctl_drive_state(&ctl, 0) == AMIGA_DRIVE_MOUNTED);

  amiga_ctl_set_probe(&ctl, probe, NULL);
  CHECK(amiga_ctl_drive_state(&ctl, 0) == AMIGA_DRIVE_SAVED);
  CHECK(amiga_ctl_drive_state(&ctl, 2) == AMIGA_DRIVE_MOUNTED);
  CHECK(amiga_ctl_drive_state(&ctl, 1) == AMIGA_DRIVE_EMPTY);
  CHECK(!amiga_ctl_drive_mounted(&ctl, 0));
  CHECK(amiga_ctl_drive_mounted(&ctl, 2));

  /* A saved-but-absent drive can't be opened; it can be remounted. */
  CHECK(!amiga_ctl_drive_window_name(&ctl, 0, name, sizeof(name)));
  CHECK_STR(state.status, "DN0: is not mounted; press Remount");
  CHECK(!amiga_ctl_drive_remount(&ctl, 1));
  CHECK_STR(state.status, "DN1: has nothing to remount");
  CHECK(amiga_ctl_drive_remount(&ctl, 0));
  CHECK_STR(last_cmd, "SYS:C/fmount 1 DN0: RO");     /* saved slot and mode */
  CHECK_STR(state.status, "Toolbox.adf mounted on DN0: (RO, slot 1)");
  CHECK(amiga_ctl_drive_state(&ctl, 0) == AMIGA_DRIVE_MOUNTED);
  CHECK(amiga_ctl_drive_window_name(&ctl, 0, name, sizeof(name)));
  CHECK_STR(name, "DN0:");

  /* The mount picker prefers a truly empty drive over a saved one. */
  present[0] = 0;
  CHECK(amiga_ctl_first_empty_drive(&ctl) == 1);
}
