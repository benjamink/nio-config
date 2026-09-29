#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"
#include "amiga_help.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;

static int no_exec(const char *command, char *output, uint16_t cap,
                   void *ctx)
{
  (void) command;
  (void) output;
  (void) cap;
  (void) ctx;
  return 20;
}

void test_ctl_help(void)
{
  fake_nio_reset();
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, no_exec, NULL);

  amiga_ctl_set_page(&ctl, AMIGA_PAGE_DRIVES);
  amiga_ctl_help_open(&ctl, AMIGA_HELP_CONTENTS);
  CHECK(ctl.page == AMIGA_PAGE_HELP && ctl.help_return == AMIGA_PAGE_DRIVES);
  CHECK(ctl.help_topic == AMIGA_HELP_CONTENTS);

  /* Moving between topics keeps the page to return to. */
  amiga_ctl_help_open(&ctl, 3);
  CHECK(ctl.help_topic == 3 && ctl.help_return == AMIGA_PAGE_DRIVES);
  amiga_ctl_help_step(&ctl, 1);
  CHECK(ctl.help_topic == 4);
  amiga_ctl_help_step(&ctl, -10);
  CHECK(ctl.help_topic == 1);              /* Previous stops at the first topic */
  amiga_ctl_help_open(&ctl, AMIGA_HELP_TOPICS - 1);
  amiga_ctl_help_step(&ctl, 1);
  CHECK(ctl.help_topic == AMIGA_HELP_TOPICS - 1);
  amiga_ctl_help_open(&ctl, 200);
  CHECK(ctl.help_topic == AMIGA_HELP_CONTENTS);

  amiga_ctl_help_close(&ctl);
  CHECK(ctl.page == AMIGA_PAGE_DRIVES);

  /* Help is only entered through help_open. */
  amiga_ctl_set_page(&ctl, AMIGA_PAGE_HELP);
  CHECK(ctl.page == AMIGA_PAGE_DRIVES);

  /* Help opened from the mount picker returns to it. */
  amiga_ctl_help_open(&ctl, 2);
  amiga_ctl_help_close(&ctl);
  CHECK(ctl.page == AMIGA_PAGE_DRIVES);
}
