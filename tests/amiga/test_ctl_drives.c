#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"
#include "amiga_drives.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;
static char last_cmd[AMIGA_CMD_MAX];
static int exec_calls;
static int exec_rc;
static int exec_records;
static const char *exec_output = "";

/* Behaves like FMOUNT/FUMOUNT + fujinet-disk.device: a successful command
 * rewrites config-nio/mappings (version byte + 8 x {flags, slot}). */
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
  strncpy(output, exec_output, cap - 1);
  output[cap - 1] = 0;
  exec_calls++;
  strcpy(last_cmd, cmd);
  memset(map, 0, sizeof(map));
  map[0] = 1;
  cur = fake_appstore_get("config-nio", "mappings", &len);
  if (cur && len == sizeof(map))
    memcpy(map, cur, sizeof(map));
  if (sscanf(cmd, "SYS:C/fmount %u %7s %3s", &slot, label, mode) == 3) {
    unit = amiga_drive_unit(label, 0);
    if (exec_records && unit >= 0) {
      map[1 + unit * 2] = (uint8_t) (1 | (mode[1] == 'O' ? 2 : 0));
      map[2 + unit * 2] = (uint8_t) slot;
    }
  } else if (sscanf(cmd, "SYS:C/fumount %d", &unit) == 1) {
    if (exec_records && unit >= 0 && unit < 8) {
      map[1 + unit * 2] = 0;
      map[2 + unit * 2] = 0;
    }
  }
  fake_appstore_put("config-nio", "mappings", map, sizeof(map));
  return exec_rc;
}

void test_ctl_drives(void)
{
  config_nio_mapping_t m;

  fake_nio_reset();
  fake_slot_put(12, "tnfs://x/boot.adf", 0);
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, fake_exec, NULL);
  exec_calls = 0;
  exec_rc = 0;
  exec_records = 1;
  strcpy(state.browse_path, "GAMES/");

  CHECK(amiga_ctl_drive_insert(&ctl, 0, 12, 1));
  CHECK_STR(last_cmd, "SYS:C/fmount 12 DN0: RO");
  CHECK(config_nio_mapping_get(&state, 0, &m) && m.valid && m.slot == 12 &&
        m.readonly);
  CHECK_STR(state.status, "Slot 12 inserted in DN0:");
  CHECK_STR(state.browse_path, "GAMES/");
  CHECK(state.host_count == 3 && ctl.hosts.count == 3);

  CHECK(!amiga_ctl_drive_insert(&ctl, 1, 13, 0));
  CHECK_STR(state.status, "Catalogue slot 13 is empty");
  CHECK(exec_calls == 1);

  exec_rc = 10;
  exec_records = 0;
  CHECK(!amiga_ctl_drive_insert(&ctl, 2, 12, 0));
  CHECK_STR(state.status, "FMOUNT failed for DN2: (rc 10)");

  /* The command's last output line explains the failure, e.g. FMOUNT not
   * installed where the FMOUNT option points. */
  exec_output = "fmount: Unknown command";
  CHECK(!amiga_ctl_drive_insert(&ctl, 2, 12, 0));
  CHECK_STR(state.status, "FMOUNT failed for DN2: fmount: Unknown command (rc 10)");
  exec_output = "";

  /* KS1.3 Execute() reports rc 0 even when FMOUNT failed: the missing
   * mapping must still be reported as a failure. */
  ctl.kick13 = 1;
  exec_rc = 0;
  CHECK(!amiga_ctl_drive_insert(&ctl, 2, 12, 0));
  CHECK_STR(last_cmd, "SYS:C/fmount 12 HN0: RW");
  CHECK_STR(state.status, "FMOUNT failed for HN0: (rc 0)");
  ctl.kick13 = 0;

  exec_records = 1;
  CHECK(amiga_ctl_drive_eject(&ctl, 0));
  CHECK_STR(last_cmd, "SYS:C/fumount 0");
  CHECK(config_nio_mapping_get(&state, 0, &m) && !m.valid);
  CHECK_STR(state.status, "DN0: ejected");
  CHECK(!amiga_ctl_drive_eject(&ctl, 0));
  CHECK_STR(state.status, "DN0: is empty");

  CHECK(!amiga_ctl_drive_insert(&ctl, 8, 12, 0));
  CHECK_STR(state.status, "No such drive");

  /* Drive rows show their mapped slot's URI: one read per drive, cached. */
  {
    static const uint8_t map[17] = { 1, 1, 0, 1, 16, 3, 32, 1, 48 };
    const config_nio_slot_t *ds;
    unsigned calls;
    uint8_t unit, pass;

    fake_slot_put(16, "tnfs://x/sixteen.adf", 0);
    fake_appstore_put("config-nio", "mappings", map, sizeof(map));
    CHECK(amiga_ctl_reload(&ctl));
    calls = fake_slot_get_calls();
    for (pass = 0; pass < 2; pass++)
      for (unit = 0; unit < 4; unit++)
        (void) amiga_ctl_drive_slot(&ctl, unit);
    CHECK(fake_slot_get_calls() == calls + 4);
    ds = amiga_ctl_drive_slot(&ctl, 1);
    CHECK(ds && !strcmp(ds->uri, "tnfs://x/sixteen.adf"));
    CHECK(amiga_ctl_drive_slot(&ctl, 5) == NULL);   /* unmapped */
  }

  /* Editor defaults: mapped drives show their mapping, anything else
   * defaults to read-only. */
  {
    uint8_t slot = 99, ro = 0;

    amiga_ctl_drive_editor(&ctl, 1, &slot, &ro);   /* mapped: slot 16 RW */
    CHECK(slot == 16 && ro == 0);
    slot = 99;
    ro = 0;
    amiga_ctl_drive_editor(&ctl, 6, &slot, &ro);   /* unmapped */
    CHECK(slot == 99 && ro == 1);
    CHECK(amiga_ctl_catalogue_readonly(&ctl, 16) == 0);  /* RW entry */
    CHECK(amiga_ctl_catalogue_readonly(&ctl, 200) == 1); /* empty slot */
  }

  amiga_ctl_set_tools(&ctl, "Work:My Tools/fmount", "SYS:C/fumount");
  CHECK(!amiga_ctl_drive_insert(&ctl, 1, 12, 0));
  CHECK_STR(state.status, "FMOUNT path is invalid");
}
