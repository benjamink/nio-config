#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"
#include "amiga_script.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;
static char transcript[1024];

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

/* The Catalogue tab lists only occupied slots, as FIN/FOUT/FMOUNT see them,
 * read with a few range requests instead of 256 single-slot reads. */
void test_ctl_catalogue_list(void)
{
  static char long_uri[200];
  uint16_t ticks = 0;
  unsigned calls;

  fake_nio_reset();
  fake_slot_put(3, "host:/Toolbox.adf", 1);
  fake_slot_put(7, "tnfs://fujinet.online/Games.adf", 0);
  memset(long_uri, 'x', sizeof(long_uri) - 1);
  memcpy(long_uri, "tnfs://h/", 9);
  strcpy(long_uri + sizeof(long_uri) - 12, "/Deep.adf");
  fake_slot_put(200, long_uri, 1);
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, no_exec, NULL);

  calls = fake_slot_get_calls();
  CHECK(amiga_ctl_catalogue_refresh(&ctl));
  CHECK(fake_slot_get_calls() == calls);           /* range reads only */
  CHECK(fake_slot_range_calls() >= 1 && fake_slot_range_calls() < 10);
  CHECK(ctl.cat_count == 3 && ctl.catalogue.count == 3);
  CHECK(ctl.cat_slot[0] == 3 && ctl.cat_slot[1] == 7 && ctl.cat_slot[2] == 200);
  CHECK(ctl.cat_ro[0] == 1 && ctl.cat_ro[1] == 0);
  CHECK_STR(ctl.cat_uri[0], "host:/Toolbox.adf");
  /* Long addresses keep their tail, where the file name is. */
  CHECK(strlen(ctl.cat_uri[2]) < AMIGA_CAT_URI_MAX + 1);
  CHECK(strstr(ctl.cat_uri[2], "/Deep.adf") != NULL);

  /* Clearing (FOUT) and setting (FIN) update the listing. */
  CHECK(amiga_ctl_slot_clear(&ctl, 7));
  CHECK(amiga_ctl_catalogue_refresh(&ctl));
  CHECK(ctl.cat_count == 2 && ctl.cat_slot[1] == 200);
  CHECK(amiga_ctl_slot_set(&ctl, 4, "host:/New.adf", 0));
  CHECK(amiga_ctl_catalogue_refresh(&ctl));
  CHECK(ctl.cat_count == 3 && ctl.cat_slot[1] == 4);
  CHECK(amiga_ctl_catalogue_index(&ctl, 200) == 2);
  CHECK(amiga_ctl_catalogue_index(&ctl, 9) == -1);

  /* Script view of the same list. */
  transcript[0] = 0;
  CHECK(amiga_script_line(&ctl, "dump catalogue", out, NULL, &ticks) ==
        AMIGA_SCRIPT_OK);
  CHECK(strstr(transcript, "SLOT 3 RO host:/Toolbox.adf\n") != NULL);
  CHECK(strstr(transcript, "SLOT 4 RW host:/New.adf\n") != NULL);
  CHECK(strstr(transcript, "OK\n") != NULL);

  /* An empty catalogue is an empty list. */
  CHECK(amiga_ctl_slot_clear(&ctl, 3) && amiga_ctl_slot_clear(&ctl, 4) &&
        amiga_ctl_slot_clear(&ctl, 200));
  CHECK(amiga_ctl_catalogue_refresh(&ctl));
  CHECK(ctl.cat_count == 0 && ctl.catalogue.selected == AMIGA_LIST_NONE);
}
