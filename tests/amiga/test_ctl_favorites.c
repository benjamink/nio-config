#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"
#include "amiga_drives.h"
#include "amiga_script.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;
static char last_cmd[AMIGA_CMD_MAX];

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
  }
  fake_appstore_put("config-nio", "mappings", map, sizeof(map));
  return 0;
}

static void discard(const char *line, void *ctx)
{
  (void) line;
  (void) ctx;
}

void test_ctl_favorites(void)
{
  static const fake_dir_entry_t root[] = {
    { CONFIG_NIO_ENTRY_FLAG_DIR, "GAMES", 0, 0 },
    { 0, "boot.adf", 901120, 0 },
    { 0, "demo.adf", 901120, 0 },
  };
  static const char stored[] =
    "tnfs://fujinet.online/demo.adf\ntnfs://fujinet.online/boot.adf\n";
  const uint8_t *v;
  uint16_t len;
  uint16_t ticks = 0;
  unsigned i;

  fake_nio_reset();
  fake_dir_put("tnfs://fujinet.online/", root, 3);
  fake_appstore_put("config-nio", "favorites",
                    "tnfs://fujinet.online/demo.adf\n", 31);
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, fake_exec, NULL);

  /* Favorites are loaded from the FujiNet, separately from the catalogue. */
  CHECK(ctl.fav_count == 1 && ctl.favorites.count == 1);
  CHECK(amiga_ctl_fav_is(&ctl, "tnfs://fujinet.online/demo.adf"));

  /* Toggle from Browse: the selected file. */
  amiga_list_select(&ctl.hosts, 2);
  CHECK(amiga_ctl_browse_open(&ctl));
  CHECK(amiga_ctl_browse_select_name(&ctl, "boot.adf"));
  CHECK(amiga_ctl_fav_toggle_browse(&ctl));
  CHECK_STR(state.status, "Added boot.adf to Favorites");
  CHECK(ctl.fav_count == 2);
  v = fake_appstore_get("config-nio", "favorites", &len);
  CHECK(v && len == strlen(stored) && !memcmp(v, stored, len));
  CHECK(amiga_ctl_fav_toggle_browse(&ctl));
  CHECK_STR(state.status, "Removed boot.adf from Favorites");
  CHECK(ctl.fav_count == 1);
  CHECK(amiga_ctl_browse_select_name(&ctl, "GAMES"));
  CHECK(!amiga_ctl_fav_toggle_browse(&ctl));
  CHECK_STR(state.status, "Pick a file, not a drawer");

  /* Mounting does not make an image a favorite (the catalogue is only
   * plumbing); mounting from Favorites returns to Favorites. */
  CHECK(amiga_ctl_browse_select_name(&ctl, "boot.adf"));
  CHECK(amiga_ctl_mount_begin_browse(&ctl));
  CHECK(amiga_ctl_mount_commit(&ctl, 0, 1));
  CHECK(!amiga_ctl_fav_is(&ctl, "tnfs://fujinet.online/boot.adf"));
  amiga_ctl_set_page(&ctl, AMIGA_PAGE_FAVORITES);
  CHECK(ctl.page == AMIGA_PAGE_FAVORITES);
  amiga_list_select(&ctl.favorites, 0);
  CHECK(amiga_ctl_mount_begin_favorite(&ctl));
  CHECK(ctl.mount_return == AMIGA_PAGE_FAVORITES);
  CHECK_STR(ctl.mount_name, "demo.adf");
  CHECK(amiga_ctl_mount_commit(&ctl, 1, 1));
  CHECK(ctl.page == AMIGA_PAGE_FAVORITES);

  /* Toggle from Drives: the image in the selected drive. */
  CHECK(amiga_ctl_fav_toggle_drive(&ctl, 0));
  CHECK_STR(state.status, "Added boot.adf to Favorites");
  CHECK(amiga_ctl_fav_is(&ctl, "tnfs://fujinet.online/boot.adf"));
  CHECK(!amiga_ctl_fav_toggle_drive(&ctl, 5));
  CHECK_STR(state.status, "DN5: is empty");

  /* Remove from the Favorites page. */
  amiga_list_select(&ctl.favorites, 0);
  CHECK(amiga_ctl_fav_remove(&ctl));
  CHECK(ctl.fav_count == 1);
  CHECK(!amiga_ctl_fav_is(&ctl, "tnfs://fujinet.online/demo.adf"));

  /* Script access: dump lists the favorites. */
  CHECK(amiga_script_line(&ctl, "dump favorites", discard, NULL, &ticks) ==
        AMIGA_SCRIPT_OK);

  /* The list is bounded. */
  for (i = 0; ctl.fav_count < AMIGA_FAV_MAX; i++) {
    char uri[40];

    sprintf(uri, "tnfs://x/%u.adf", i);
    CHECK(amiga_ctl_fav_add(&ctl, uri));
  }
  CHECK(!amiga_ctl_fav_add(&ctl, "tnfs://x/one-more.adf"));
  CHECK_STR(state.status, "Favorites are full");
}
