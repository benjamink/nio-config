#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"

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

void test_ctl_hosts(void)
{
  static const char seeded[] =
    "sd0:/\nfujinet.diller.org\nfujinet.online\ntnfs://example.org\n";
  const uint8_t *v;
  uint16_t len;

  fake_nio_reset();
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, no_exec, NULL);

  /* First load seeds the same three hosts as every other target. */
  CHECK(state.host_count == 3);
  CHECK(ctl.hosts.count == 3 && ctl.hosts.selected == 0);
  CHECK(ctl.catalogue.count == 256 && ctl.drives.count == 8);

  CHECK(amiga_ctl_host_add(&ctl, "tnfs://example.org"));
  CHECK(state.host_count == 4 && ctl.hosts.selected == 3);
  CHECK_STR(state.status, "Host added");
  v = fake_appstore_get("config-nio", "hosts", &len);
  CHECK(v && len == strlen(seeded) && !memcmp(v, seeded, len));

  CHECK(!amiga_ctl_host_add(&ctl, ""));
  CHECK_STR(state.status, "Host is empty");

  amiga_list_select(&ctl.hosts, 1);
  CHECK(amiga_ctl_host_move(&ctl, -1));
  CHECK_STR(state.hosts[0], "fujinet.diller.org");
  CHECK(ctl.hosts.selected == 0);
  CHECK(!amiga_ctl_host_move(&ctl, -1));

  CHECK(amiga_ctl_host_replace(&ctl, "tnfs://replaced"));
  CHECK_STR(state.hosts[0], "tnfs://replaced");
  CHECK(!amiga_ctl_host_replace(&ctl, ""));

  amiga_list_select(&ctl.hosts, 3);
  CHECK(amiga_ctl_host_remove(&ctl));
  CHECK(state.host_count == 3 && ctl.hosts.selected == 2);

  while (state.host_count < CONFIG_NIO_MAX_HOSTS)
    CHECK(amiga_ctl_host_add(&ctl, "tnfs://h"));
  CHECK(!amiga_ctl_host_add(&ctl, "tnfs://one-too-many"));
  CHECK_STR(state.status, "Host list is full");

  while (state.host_count)
    CHECK(amiga_ctl_host_remove(&ctl));
  CHECK(ctl.hosts.selected == AMIGA_LIST_NONE);
  CHECK(!amiga_ctl_host_remove(&ctl));
  CHECK_STR(state.status, "No host selected");
  CHECK(!amiga_ctl_host_move(&ctl, 1));

  CHECK(amiga_ctl_set_prefs(&ctl, CONFIG_NIO_PREF_DATE_YDM,
                            CONFIG_NIO_PREF_SIZE_COMPACT));
  v = fake_appstore_get("config-nio", "prefs", &len);
  CHECK(v && len >= 22 && !memcmp(v, "date=ydm\nsize=compact\n", 22));
  CHECK(state.prefs.date_format == CONFIG_NIO_PREF_DATE_YDM);

  amiga_ctl_set_page(&ctl, AMIGA_PAGE_DRIVES);
  CHECK(ctl.page == AMIGA_PAGE_DRIVES);
  amiga_ctl_set_page(&ctl, 9);
  CHECK(ctl.page == AMIGA_PAGE_DRIVES);
}
