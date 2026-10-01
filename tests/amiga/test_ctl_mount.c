#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"
#include "amiga_drives.h"
#include "amiga_script.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;
static char last_cmd[AMIGA_CMD_MAX];
static int exec_rc;
static int exec_records;

/* FMOUNT/FUMOUNT stand-in: rewrites config-nio/mappings like the device. */
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
  if (sscanf(cmd, "%*s %u %7s %3s", &slot, label, mode) == 3) {
    unit = amiga_drive_unit(label, 0);
    if (exec_records && unit >= 0) {
      map[1 + unit * 2] = (uint8_t) (1 | (mode[1] == 'O' ? 2 : 0));
      map[2 + unit * 2] = (uint8_t) slot;
    }
  }
  fake_appstore_put("config-nio", "mappings", map, sizeof(map));
  return exec_rc;
}

static void discard(const char *line, void *ctx)
{
  (void) line;
  (void) ctx;
}

static void open_root(void)
{
  amiga_ctl_set_page(&ctl, AMIGA_PAGE_HOSTS);
  amiga_list_select(&ctl.hosts, 2);   /* fujinet.online */
  CHECK(amiga_ctl_browse_open(&ctl));
}

void test_ctl_mount(void)
{
  static const fake_dir_entry_t root[] = {
    { CONFIG_NIO_ENTRY_FLAG_DIR, "GAMES", 0, 0 },
    { 0, "boot.adf", 901120, 0 },
    { 0, "new.adf", 901120, 0 },
  };
  static char uris[256][24];
  uint16_t ticks = 0;
  unsigned i;

  fake_nio_reset();
  fake_dir_put("tnfs://fujinet.online/", root, 3);
  fake_slot_put(0, "tnfs://x/a.adf", 0);
  fake_slot_put(1, "tnfs://fujinet.online/boot.adf", 0);
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, fake_exec, NULL);
  exec_rc = 0;
  exec_records = 1;
  CHECK(amiga_ctl_first_empty_drive(&ctl) == 0);

  /* Browse -> Mount: preselects the first empty drive, RO by default. */
  open_root();
  CHECK(amiga_ctl_browse_select_name(&ctl, "boot.adf"));
  CHECK(amiga_ctl_mount_begin_browse(&ctl));
  CHECK(ctl.page == AMIGA_PAGE_MOUNT && ctl.mount_return == AMIGA_PAGE_BROWSE);
  CHECK(ctl.drives.selected == 0);
  CHECK_STR(ctl.mount_name, "boot.adf");
  CHECK_STR(state.status, "Choose a drive for boot.adf");

  /* Reuses the catalogue slot that already holds the image; the RO choice
   * is written to that entry and passed to FMOUNT. */
  CHECK(amiga_ctl_mount_commit(&ctl, 0, 1));
  CHECK_STR(last_cmd, "fmount 1 DN0: RO");
  CHECK(fake_slot_readonly(1) == 1);
  CHECK(ctl.page == AMIGA_PAGE_BROWSE);
  CHECK_STR(state.status, "boot.adf mounted on DN0: (RO, slot 1)");
  CHECK(amiga_ctl_drive_mounted(&ctl, 0));
  CHECK(amiga_ctl_first_empty_drive(&ctl) == 1);

  /* Double-clicking a drive opens it like its Workbench icon: by name. */
  {
    char name[8];

    CHECK(amiga_ctl_drive_window_name(&ctl, 0, name, sizeof(name)));
    CHECK_STR(name, "DN0:");
    CHECK(!amiga_ctl_drive_window_name(&ctl, 5, name, sizeof(name)));
    CHECK_STR(state.status, "DN5: is empty");
    ctl.kick13 = 1;
    CHECK(amiga_ctl_drive_window_name(&ctl, 0, name, sizeof(name)));
    CHECK_STR(name, "DN0:");
    ctl.kick13 = 0;
  }

  /* A new image takes the first free slot and may replace a mount. */
  open_root();
  CHECK(amiga_ctl_browse_select_name(&ctl, "new.adf"));
  CHECK(amiga_ctl_mount_begin_browse(&ctl));
  CHECK(ctl.drives.selected == 1);
  CHECK(amiga_ctl_mount_commit(&ctl, 0, 0));
  CHECK_STR(fake_slot_uri(2), "tnfs://fujinet.online/new.adf");
  CHECK(fake_slot_readonly(2) == 0);
  CHECK_STR(last_cmd, "fmount 2 DN0: RW");
  CHECK_STR(state.status, "new.adf mounted on DN0: (RW, slot 2)");

  /* Drawers cannot be mounted; the page does not change. */
  open_root();
  CHECK(amiga_ctl_browse_select_name(&ctl, "GAMES"));
  CHECK(!amiga_ctl_mount_begin_browse(&ctl));
  CHECK(ctl.page == AMIGA_PAGE_BROWSE);
  CHECK_STR(state.status, "Pick a file, not a drawer");

  /* Cancel returns to where the user came from. */
  CHECK(amiga_ctl_browse_select_name(&ctl, "boot.adf"));
  CHECK(amiga_ctl_mount_begin_browse(&ctl));
  amiga_ctl_mount_cancel(&ctl);
  CHECK(ctl.page == AMIGA_PAGE_BROWSE);

  /* A failed FMOUNT keeps the drive list open so another drive can be
   * chosen. */
  CHECK(amiga_ctl_mount_begin_browse(&ctl));
  exec_rc = 10;
  exec_records = 0;
  CHECK(!amiga_ctl_mount_commit(&ctl, 3, 1));
  CHECK(ctl.page == AMIGA_PAGE_MOUNT);
  CHECK_STR(state.status, "FMOUNT failed for DN3: (rc 10)");
  amiga_ctl_mount_cancel(&ctl);
  exec_rc = 0;
  exec_records = 1;

  /* Catalogue source: mounts that slot directly. */
  amiga_ctl_set_page(&ctl, AMIGA_PAGE_CATALOGUE);
  CHECK(!amiga_ctl_mount_begin_slot(&ctl, 5));
  CHECK_STR(state.status, "Catalogue slot 5 is empty");
  CHECK(amiga_ctl_mount_begin_slot(&ctl, 0));
  CHECK(ctl.mount_return == AMIGA_PAGE_CATALOGUE);
  CHECK_STR(ctl.mount_name, "a.adf");
  CHECK(amiga_ctl_mount_commit(&ctl, 1, 1));
  CHECK_STR(last_cmd, "fmount 0 DN1: RO");
  CHECK(fake_slot_readonly(0) == 1);
  CHECK(ctl.page == AMIGA_PAGE_CATALOGUE);

  /* Script form uses the browse selection. */
  open_root();
  CHECK(amiga_ctl_browse_select_name(&ctl, "new.adf"));
  CHECK(amiga_script_line(&ctl, "mount DN2: ro", discard, NULL, &ticks) ==
        AMIGA_SCRIPT_OK);
  CHECK_STR(last_cmd, "fmount 2 DN2: RO");

  /* The mount page is entered only through mount_begin_*. */
  amiga_ctl_set_page(&ctl, AMIGA_PAGE_MOUNT);
  CHECK(ctl.page == AMIGA_PAGE_BROWSE);

  /* Full catalogue: a new image cannot be mounted. */
  for (i = 0; i < 256; i++) {
    sprintf(uris[i], "tnfs://x/%u.adf", i);
    fake_slot_put((uint8_t) i, uris[i], 0);
  }
  amiga_ctl_reload(&ctl);
  open_root();
  CHECK(amiga_ctl_browse_select_name(&ctl, "new.adf"));
  CHECK(amiga_ctl_mount_begin_browse(&ctl));
  CHECK(!amiga_ctl_mount_commit(&ctl, 4, 1));
  CHECK_STR(state.status, "Catalogue is full");
}
