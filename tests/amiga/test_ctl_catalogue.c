#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;

static int no_exec(const char *command, void *ctx)
{
  (void) command;
  (void) ctx;
  return 20;
}

void test_ctl_catalogue(void)
{
  const config_nio_slot_t *s;
  unsigned calls;

  fake_nio_reset();
  fake_slot_put(3, "tnfs://x/a.adf", 1);
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, no_exec, NULL);

  calls = fake_slot_get_calls();
  s = amiga_ctl_slot(&ctl, 3);
  CHECK(s && s->enabled && !strcmp(s->uri, "tnfs://x/a.adf") &&
        !strcmp(s->mode, "r"));
  CHECK(fake_slot_get_calls() == calls + AMIGA_CAT_WINDOW);
  s = amiga_ctl_slot(&ctl, 10);
  CHECK(s && !s->enabled);
  CHECK(fake_slot_get_calls() == calls + AMIGA_CAT_WINDOW);
  CHECK(amiga_ctl_slot(&ctl, 16) != NULL);
  CHECK(fake_slot_get_calls() == calls + 2 * AMIGA_CAT_WINDOW);
  CHECK(amiga_ctl_slot(&ctl, 255) != NULL);

  CHECK(amiga_ctl_slot_set(&ctl, 20, "tnfs://x/b.adf", 0));
  CHECK_STR(fake_slot_uri(20), "tnfs://x/b.adf");
  CHECK(fake_slot_readonly(20) == 0);
  CHECK_STR(state.status, "Slot 20 saved");
  s = amiga_ctl_slot(&ctl, 20);
  CHECK(s && !strcmp(s->uri, "tnfs://x/b.adf") && !strcmp(s->mode, "rw"));

  CHECK(!amiga_ctl_slot_set(&ctl, 21, "", 0));
  CHECK_STR(state.status, "URI is empty");

  CHECK(amiga_ctl_slot_clear(&ctl, 3));
  CHECK(fake_slot_uri(3) == NULL);
  CHECK_STR(state.status, "Slot 3 cleared");
  s = amiga_ctl_slot(&ctl, 3);
  CHECK(s && !s->enabled);
}
