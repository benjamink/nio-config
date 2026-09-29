#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;
static char many_names[250][8];
static fake_dir_entry_t many[250];

static int no_exec(const char *command, char *output, uint16_t cap,
                   void *ctx)
{
  (void) command;
  (void) output;
  (void) cap;
  (void) ctx;
  return 20;
}

void test_ctl_browse(void)
{
  static const fake_dir_entry_t root[] = {
    { CONFIG_NIO_ENTRY_FLAG_DIR, "GAMES", 0, 0 },
    { 0, "boot.adf", 901120, 1735689600UL },
    { CONFIG_NIO_ENTRY_FLAG_NAME_TRUNCATED, "very_long_na", 1, 1 },
  };
  static const fake_dir_entry_t games[] = { { 0, "a.adf", 880, 0 } };
  char uri[CONFIG_NIO_URI_MAX + 1];
  unsigned i;

  CHECK(CONFIG_NIO_MAX_ENTRIES == 200);
  fake_nio_reset();
  fake_dir_put("tnfs://fujinet.online/", root, 3);
  fake_dir_put("tnfs://fujinet.online/GAMES/", games, 1);
  fake_dir_fail("tnfs://fujinet.diller.org/");
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, no_exec, NULL);

  amiga_list_select(&ctl.hosts, 2);
  CHECK(amiga_ctl_browse_open(&ctl));
  CHECK(ctl.page == AMIGA_PAGE_BROWSE && ctl.browse_open);
  CHECK(ctl.entries.count == 3 && ctl.entries.selected == 0);
  CHECK_STR(state.status, "Entries loaded");

  CHECK(amiga_ctl_browse_select_name(&ctl, "boot.adf"));
  CHECK(amiga_ctl_browse_uri(&ctl, uri, sizeof(uri)));
  CHECK_STR(uri, "tnfs://fujinet.online/boot.adf");
  CHECK(!amiga_ctl_browse_activate(&ctl));
  CHECK_STR(state.status, "Press Mount... to mount this image");
  CHECK(amiga_ctl_browse_assign(&ctl, 12, 1));
  CHECK_STR(fake_slot_uri(12), "tnfs://fujinet.online/boot.adf");
  CHECK(fake_slot_readonly(12) == 1);
  CHECK_STR(state.status, "Assigned to slot 12");

  CHECK(amiga_ctl_browse_select_name(&ctl, "very_long_na"));
  CHECK(!amiga_ctl_browse_activate(&ctl));
  CHECK_STR(state.status, "Name too long for this client");
  CHECK(!amiga_ctl_browse_assign(&ctl, 13, 0));
  CHECK(fake_slot_uri(13) == NULL);

  CHECK(amiga_ctl_browse_select_name(&ctl, "GAMES"));
  CHECK(!amiga_ctl_browse_assign(&ctl, 13, 0));
  CHECK_STR(state.status, "Pick a file, not a drawer");
  CHECK(amiga_ctl_browse_activate(&ctl));
  CHECK_STR(state.browse_path, "GAMES/");
  CHECK(ctl.entries.count == 1);
  CHECK(amiga_ctl_browse_parent(&ctl));
  CHECK_STR(state.browse_path, "");
  CHECK(ctl.entries.count == 3);
  CHECK(!amiga_ctl_browse_parent(&ctl));
  CHECK(!amiga_ctl_browse_select_name(&ctl, "nope"));

  /* Unreachable host: page stays usable, nothing selectable. */
  amiga_list_select(&ctl.hosts, 1);
  CHECK(!amiga_ctl_browse_open(&ctl));
  CHECK(ctl.page == AMIGA_PAGE_BROWSE);
  CHECK(ctl.entries.count == 0);
  CHECK(!strncmp(state.status, "Browse failed: error ", 21));
  CHECK(!amiga_ctl_browse_activate(&ctl));
  CHECK_STR(state.status, "Nothing selected");

  /* Directory larger than the table. */
  for (i = 0; i < 250; i++) {
    sprintf(many_names[i], "F%03u", i);
    many[i].is_dir = 0;
    many[i].name = many_names[i];
    many[i].size = i;
    many[i].mtime = 0;
  }
  fake_dir_put("sd0:/", many, 250);
  amiga_list_select(&ctl.hosts, 0);
  CHECK(amiga_ctl_browse_open(&ctl));
  CHECK(ctl.entries.count == 200);
  CHECK_STR(state.status, "Showing 200 of 250 entries");

  /* No host at all. */
  while (state.host_count)
    CHECK(amiga_ctl_host_remove(&ctl));
  CHECK(!amiga_ctl_browse_open(&ctl));
  CHECK_STR(state.status, "No host selected");
}
