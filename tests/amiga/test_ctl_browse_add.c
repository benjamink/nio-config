#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"
#include "amiga_script.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;
static char transcript[512];

static int no_exec(const char *command, char *output, uint16_t cap,
                   void *ctx)
{
  (void) command;
  (void) output;
  (void) cap;
  (void) ctx;
  return 20;
}

static void out(const char *line, void *ctx)
{
  (void) ctx;
  strcat(transcript, line);
  strcat(transcript, "\n");
}

/* Browse > Add to Slot is FIN without a slot number: the image goes in the
 * slot that already holds it, else the first empty one. */
void test_ctl_browse_add(void)
{
  static const fake_dir_entry_t root[] = {
    { CONFIG_NIO_ENTRY_FLAG_DIR, "GAMES", 0, 0 },
    { 0, "boot.adf", 901120, 0 },
    { 0, "work.adf", 901120, 0 },
  };
  uint16_t ticks = 0;
  uint16_t i;

  fake_nio_reset();
  fake_dir_put("tnfs://fujinet.online/", root, 3);
  fake_slot_put(0, "host:/Toolbox.adf", 1);
  fake_slot_put(2, "host:/Other.adf", 0);
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, no_exec, NULL);
  amiga_list_select(&ctl.hosts, 2);
  CHECK(amiga_ctl_browse_open(&ctl));

  /* First empty slot, read-only by default in the GUI. */
  CHECK(amiga_ctl_browse_select_name(&ctl, "boot.adf"));
  CHECK(amiga_ctl_browse_add(&ctl, 1));
  CHECK_STR(fake_slot_uri(1), "tnfs://fujinet.online/boot.adf");
  CHECK(fake_slot_readonly(1) == 1);
  CHECK_STR(state.status, "boot.adf added to slot 1 (RO)");
  CHECK(ctl.page == AMIGA_PAGE_BROWSE);

  /* Adding it again reuses its slot instead of taking another. */
  CHECK(amiga_ctl_browse_add(&ctl, 1));
  CHECK(fake_slot_uri(3) == NULL);
  CHECK_STR(state.status, "boot.adf is already in slot 1 (RO)");

  /* A different mode updates that slot. */
  CHECK(amiga_ctl_browse_add(&ctl, 0));
  CHECK(fake_slot_readonly(1) == 0);
  CHECK_STR(state.status, "boot.adf added to slot 1 (RW)");

  /* The next image skips the occupied slots 0-2. */
  CHECK(amiga_ctl_browse_select_name(&ctl, "work.adf"));
  CHECK(amiga_ctl_browse_add(&ctl, 1));
  CHECK_STR(fake_slot_uri(3), "tnfs://fujinet.online/work.adf");

  /* The Catalogue tab shows it. */
  CHECK(amiga_ctl_catalogue_refresh(&ctl));
  CHECK(amiga_ctl_catalogue_index(&ctl, 3) == 3);

  /* Drawers and a missing selection are refused. */
  CHECK(amiga_ctl_browse_select_name(&ctl, "GAMES"));
  CHECK(!amiga_ctl_browse_add(&ctl, 1));
  CHECK_STR(state.status, "Pick a file, not a drawer");

  /* SCRIPT: assign without a slot number does the same. */
  CHECK(amiga_ctl_browse_select_name(&ctl, "work.adf"));
  transcript[0] = 0;
  CHECK(amiga_script_line(&ctl, "assign ro", out, NULL, &ticks) ==
        AMIGA_SCRIPT_OK);
  CHECK(strstr(transcript, "OK\n") != NULL);
  CHECK_STR(state.status, "work.adf is already in slot 3 (RO)");

  /* A full catalogue is reported, not overwritten. */
  for (i = 4; i < 256; i++)
    fake_slot_put((uint8_t) i, "host:/Filler.adf", 1);
  CHECK(amiga_ctl_browse_select_name(&ctl, "boot.adf"));
  CHECK(amiga_ctl_browse_add(&ctl, 1));            /* already in slot 1 */
  fake_slot_put(1, "host:/Filler.adf", 1);
  CHECK(!amiga_ctl_browse_add(&ctl, 1));
  CHECK_STR(state.status, "Catalogue is full");
}
