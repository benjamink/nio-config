#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"
#include "amiga_drives.h"
#include "amiga_script.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;
static char transcript[4096];

static void out(const char *line, void *ctx)
{
  (void) ctx;
  strcat(transcript, line);
  strcat(transcript, "\n");
}

static int ok_exec(const char *cmd, void *ctx)
{
  uint8_t map[17];
  unsigned slot;
  char label[8];
  char mode[4];
  int unit;

  (void) ctx;
  memset(map, 0, sizeof(map));
  map[0] = 1;
  if (sscanf(cmd, "SYS:C/fmount %u %7s %3s", &slot, label, mode) == 3 &&
      (unit = amiga_drive_unit(label, 0)) >= 0) {
    map[1 + unit * 2] = (uint8_t) (1 | (mode[1] == 'O' ? 2 : 0));
    map[2 + unit * 2] = (uint8_t) slot;
  }
  fake_appstore_put("config-nio", "mappings", map, sizeof(map));
  return 0;
}

static int run(const char *line)
{
  uint16_t ticks = 0;
  return amiga_script_line(&ctl, line, out, NULL, &ticks);
}

void test_script(void)
{
  uint16_t ticks = 0;

  fake_nio_reset();
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, ok_exec, NULL);
  transcript[0] = 0;

  CHECK(run("page drives") == AMIGA_SCRIPT_OK && ctl.page == AMIGA_PAGE_DRIVES);
  CHECK(run("page nope") == AMIGA_SCRIPT_ERR);
  CHECK(run("host add tnfs://example") == AMIGA_SCRIPT_OK);
  CHECK(run("dump hosts") == AMIGA_SCRIPT_OK);
  CHECK(run("slot set 12 tnfs://x/boot.adf RO") == AMIGA_SCRIPT_OK);
  CHECK(run("dump slot 12") == AMIGA_SCRIPT_OK);
  CHECK(run("dump slot 13") == AMIGA_SCRIPT_OK);
  CHECK(run("insert 12 DN0: ro") == AMIGA_SCRIPT_OK);
  CHECK(run("dump drives") == AMIGA_SCRIPT_OK);
  CHECK(run("insert 12") == AMIGA_SCRIPT_ERR);
  CHECK(run("; comment") == AMIGA_SCRIPT_OK);
  CHECK(run("") == AMIGA_SCRIPT_OK);
  CHECK(run("frobnicate") == AMIGA_SCRIPT_ERR);
  CHECK(amiga_script_line(&ctl, "wait 50", out, NULL, &ticks) ==
        AMIGA_SCRIPT_WAIT && ticks == 50);
  CHECK(run("quit") == AMIGA_SCRIPT_QUIT);

  CHECK_STR(transcript,
    "OK\n"
    "ERR Unknown page\n"
    "OK\n"
    "HOST 0 sd0:/\n"
    "HOST 1 fujinet.diller.org\n"
    "HOST 2 fujinet.online\n"
    "HOST 3 tnfs://example\n"
    "OK\n"
    "OK\n"
    "SLOT 12 RO tnfs://x/boot.adf\n"
    "OK\n"
    "SLOT 13 EMPTY\n"
    "OK\n"
    "OK\n"
    "DRIVE DN0: 12 RO\n"
    "DRIVE DN1: - -\n"
    "DRIVE DN2: - -\n"
    "DRIVE DN3: - -\n"
    "DRIVE DN4: - -\n"
    "DRIVE DN5: - -\n"
    "DRIVE DN6: - -\n"
    "DRIVE DN7: - -\n"
    "OK\n"
    "ERR Bad arguments\n"
    "ERR Unknown command\n");
}
