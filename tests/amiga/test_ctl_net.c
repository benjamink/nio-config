#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"
#include "amiga_net.h"
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

static int no_exec(const char *command, char *output, uint16_t cap,
                   void *ctx)
{
  (void) command;
  (void) output;
  (void) cap;
  (void) ctx;
  return 20;
}

static int run(const char *line)
{
  uint16_t ticks = 0;

  transcript[0] = 0;
  return amiga_script_line(&ctl, line, out, NULL, &ticks);
}

static void setup(void)
{
  fake_nio_reset();
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, no_exec, NULL);
}

static void test_refresh(void)
{
  setup();
  CHECK(amiga_ctl_net_refresh(&ctl));
  CHECK(ctl.net.have_status && ctl.net.have_config && ctl.net.have_adapter);
  CHECK_STR(state.status, "Connected: home");
  CHECK(ctl.netinfo.count == AMIGA_NET_ROWS);
  CHECK(amiga_ctl_net_poll(&ctl) == 2);

  /* Firmware without GET_ADAPTER_INFO still shows the Wi-Fi details. */
  fake_wifi_adapter_error(FN_ERR_UNSUPPORTED);
  CHECK(amiga_ctl_net_refresh(&ctl));
  CHECK(!ctl.net.have_adapter && ctl.net.adapter_error == FN_ERR_UNSUPPORTED);

  /* No Wi-Fi service at all. */
  fake_wifi_status_error(FN_ERR_UNSUPPORTED);
  CHECK(!amiga_ctl_net_refresh(&ctl));
  CHECK_STR(state.status, "This FujiNet does not report Wi-Fi status");
  CHECK(amiga_ctl_net_poll(&ctl) == 0xFF);
  fake_wifi_status_error(FN_ERR_IO);
  CHECK(!amiga_ctl_net_refresh(&ctl));
  CHECK(strstr(state.status, "Wi-Fi status failed") != NULL);
}

static void test_scan_and_pick(void)
{
  char name[16];
  int i;

  setup();
  amiga_ctl_set_page(&ctl, AMIGA_PAGE_NETWORK);
  /* 20 networks arrive over three scan pages (at most 9 per reply). */
  for (i = 0; i < 20; i++) {
    sprintf(name, "net%02d", i);
    fake_wifi_add_network(name, (int8_t) (-90 + i), (uint8_t) (i & 1));
  }
  fake_wifi_add_network("home", -70, 1);
  CHECK(amiga_ctl_wifi_begin(&ctl));
  CHECK(ctl.page == AMIGA_PAGE_WIFI);
  CHECK(ctl.net.scan_count == 21 && ctl.networks.count == 21);
  CHECK(fake_wifi_scan_calls() == 3);
  CHECK(strstr(state.status, "Found 21 networks") != NULL);
  /* The saved network is selected even though others are stronger. */
  CHECK(ctl.networks.selected == 20);

  /* A tab cannot be changed under the picker; Cancel goes back. */
  amiga_ctl_set_page(&ctl, AMIGA_PAGE_HOSTS);
  CHECK(ctl.page == AMIGA_PAGE_WIFI);
  amiga_ctl_wifi_cancel(&ctl);
  CHECK(ctl.page == AMIGA_PAGE_NETWORK);

  /* Hidden networks (no name) are left out; paging still reads past
   * them, so names after a hidden one are kept. */
  setup();
  for (i = 0; i < 12; i++) {
    sprintf(name, i % 4 == 1 ? "" : "n%02d", i);
    fake_wifi_add_network(name, -60, 1);
  }
  CHECK(amiga_ctl_wifi_begin(&ctl));
  CHECK(ctl.net.scan_count == 9 && ctl.networks.count == 9);
  CHECK_STR(ctl.net.scan[0].ssid, "n00");
  CHECK_STR(ctl.net.scan[1].ssid, "n02");
  CHECK_STR(ctl.net.scan[8].ssid, "n11");
  CHECK(fake_wifi_scan_calls() == 2);

  /* Without the saved network, the strongest is selected. */
  setup();
  fake_wifi_add_network("weak", -85, 1);
  fake_wifi_add_network("strong", -40, 1);
  CHECK(amiga_ctl_wifi_begin(&ctl));
  CHECK(ctl.networks.selected == 1);

  /* Nothing in range is not an error. */
  setup();
  CHECK(amiga_ctl_wifi_begin(&ctl));
  CHECK(ctl.net.scan_count == 0);
  CHECK(strstr(state.status, "No networks found") != NULL);

  /* Scan failures leave the Network page showing. */
  setup();
  amiga_ctl_set_page(&ctl, AMIGA_PAGE_NETWORK);
  fake_wifi_scan_error(FN_ERR_UNSUPPORTED);
  CHECK(!amiga_ctl_wifi_begin(&ctl));
  CHECK(ctl.page == AMIGA_PAGE_NETWORK);
  CHECK_STR(state.status, "This FujiNet cannot scan for networks");

  /* A host-managed adapter (POSIX host mode) cannot change networks. */
  setup();
  fake_wifi_status()->capabilities = FN_WIFI_CAP_CONFIG | FN_WIFI_CAP_STATUS |
                                     FN_WIFI_CAP_HOST;
  fake_wifi_add_network("other", -50, 1);
  CHECK(!amiga_ctl_wifi_begin(&ctl));
  CHECK_STR(state.status, "Wi-Fi is managed by the host computer");
  CHECK(fake_wifi_scan_calls() == 0);
}

static void test_join(void)
{
  setup();
  amiga_ctl_set_page(&ctl, AMIGA_PAGE_NETWORK);
  fake_wifi_add_network("cafe", -60, 0);
  fake_wifi_add_network("office", -50, 1);
  fake_wifi_add_network("home", -70, 1);
  CHECK(amiga_ctl_wifi_begin(&ctl));

  /* What Join needs, per row. */
  CHECK(!amiga_ctl_wifi_needs_pass(&ctl, 0));
  CHECK(amiga_ctl_wifi_needs_pass(&ctl, 1));
  CHECK(!amiga_ctl_wifi_needs_pass(&ctl, 2));   /* saved, passphrase stored */
  amiga_ctl_wifi_hint(&ctl, 0);
  CHECK_STR(state.status, "Open network: press Join");
  amiga_ctl_wifi_hint(&ctl, 1);
  CHECK_STR(state.status, "Secured network: press Join to enter the passphrase");
  amiga_ctl_wifi_hint(&ctl, 2);
  CHECK_STR(state.status, "Saved network: press Join to reconnect");
  CHECK(!amiga_ctl_wifi_is_saved(&ctl, 1) && amiga_ctl_wifi_is_saved(&ctl, 2));
  CHECK(!amiga_ctl_wifi_is_saved(&ctl, 99));

  /* A secured network needs a passphrase of 8-64 characters; nothing is
   * written until it has one. */
  CHECK(!amiga_ctl_wifi_commit(&ctl, 1, ""));
  CHECK_STR(state.status, "Type the network's passphrase first");
  CHECK(!amiga_ctl_wifi_commit(&ctl, 1, "short"));
  CHECK_STR(state.status, "Passphrase must be 8 to 64 characters");
  CHECK(!amiga_ctl_wifi_commit(&ctl, 1,
        "0123456789012345678901234567890123456789012345678901234567890123x"));
  CHECK(fake_wifi_set_calls() == 0);
  CHECK(ctl.page == AMIGA_PAGE_WIFI);

  /* Join saves SSID and passphrase, clears a pinned BSSID, enables Wi-Fi,
   * persists and reconnects, then returns to the Network page. */
  strcpy(fake_wifi_config()->bssid, "aa:bb:cc:dd:ee:ff");
  CHECK(amiga_ctl_wifi_commit(&ctl, 1, "correct horse"));
  CHECK(fake_wifi_set_calls() == 1);
  CHECK(fake_wifi_last_fields() ==
        (FN_WIFI_SET_ENABLED | FN_WIFI_SET_SSID | FN_WIFI_SET_BSSID |
         FN_WIFI_SET_PASSWORD | FN_WIFI_SET_PERSIST | FN_WIFI_SET_RECONNECT));
  CHECK_STR(fake_wifi_config()->ssid, "office");
  CHECK_STR(fake_wifi_password(), "correct horse");
  CHECK_STR(fake_wifi_bssid(), "");
  CHECK(ctl.page == AMIGA_PAGE_NETWORK);
  CHECK(strstr(state.status, "Saved office") != NULL);
  CHECK(ctl.net.status.link_state == 1);

  /* The result after waiting: connecting, connected or failed. */
  CHECK(amiga_ctl_net_join_result(&ctl, "office") == 1);
  CHECK(strstr(state.status, "Still connecting to office") != NULL);
  fake_wifi_status()->link_state = 2;
  CHECK(amiga_ctl_net_join_result(&ctl, "office") == 2);
  CHECK_STR(state.status, "Connected to office, address 192.168.1.50");
  fake_wifi_status()->link_state = 3;
  CHECK(amiga_ctl_net_join_result(&ctl, "office") == 3);
  CHECK(strstr(state.status, "check the passphrase") != NULL);

  /* Rejoining the saved network with nothing typed keeps its stored
   * passphrase: the password field is not sent. */
  CHECK(amiga_ctl_wifi_begin(&ctl));
  CHECK(amiga_ctl_wifi_commit(&ctl, 1, ""));
  CHECK(!(fake_wifi_last_fields() & FN_WIFI_SET_PASSWORD));
  CHECK_STR(fake_wifi_password(), "correct horse");

  /* An open network is joined with an empty passphrase. */
  CHECK(amiga_ctl_wifi_begin(&ctl));
  CHECK(amiga_ctl_wifi_commit(&ctl, 0, "ignored"));
  CHECK_STR(fake_wifi_config()->ssid, "cafe");
  CHECK_STR(fake_wifi_password(), "");
  CHECK(!fake_wifi_config()->password_present);

  /* A failed save stays on the picker with the reason. */
  CHECK(amiga_ctl_wifi_begin(&ctl));
  fake_wifi_set_error(FN_ERR_NOT_READY);
  CHECK(!amiga_ctl_wifi_commit(&ctl, 1, "correct horse"));
  CHECK(ctl.page == AMIGA_PAGE_WIFI);
  CHECK_STR(state.status, "The FujiNet could not save the network");

  /* No selection. */
  CHECK(!amiga_ctl_wifi_commit(&ctl, 99, "correct horse"));
  CHECK_STR(state.status, "Choose a network first");
}

static void test_script(void)
{
  setup();
  fake_wifi_add_network("office", -50, 1);
  fake_wifi_add_network("cafe", -60, 0);

  CHECK(run("page network") == AMIGA_SCRIPT_OK);
  CHECK(ctl.page == AMIGA_PAGE_NETWORK);

  CHECK(run("wifi status") == AMIGA_SCRIPT_OK);
  CHECK(strstr(transcript, "NET Wi-Fi         Connected\n") != NULL);
  CHECK(strstr(transcript, "NET MAC address   24:6F:28:AB:CD:EF\n") != NULL);
  CHECK(strstr(transcript, "NET Firmware      0.1.1\n") != NULL);
  CHECK(strstr(transcript, "\nOK\n") != NULL);
  CHECK(run("dump network") == AMIGA_SCRIPT_OK);
  CHECK(strstr(transcript, "NET IP address    192.168.1.50\n") != NULL);

  /* connect needs a scan first. */
  CHECK(run("wifi connect 0 secret-pass") == AMIGA_SCRIPT_ERR);
  CHECK(strstr(transcript, "ERR Run wifi scan first") != NULL);

  CHECK(run("wifi scan") == AMIGA_SCRIPT_OK);
  CHECK(strstr(transcript, "NETWORK 0 -50 SECURED office\n") != NULL);
  CHECK(strstr(transcript, "NETWORK 1 -60 OPEN cafe\n") != NULL);
  CHECK(ctl.page == AMIGA_PAGE_WIFI);

  /* The passphrase is the rest of the line, spaces included. */
  CHECK(run("wifi connect 0 correct horse battery") == AMIGA_SCRIPT_OK);
  CHECK_STR(fake_wifi_config()->ssid, "office");
  CHECK_STR(fake_wifi_password(), "correct horse battery");
  CHECK(ctl.page == AMIGA_PAGE_NETWORK);

  /* join names any network, hidden ones included. */
  CHECK(run("wifi join hidden-net s3cret-passphrase") == AMIGA_SCRIPT_OK);
  CHECK_STR(fake_wifi_config()->ssid, "hidden-net");
  CHECK_STR(fake_wifi_password(), "s3cret-passphrase");
  /* No passphrase: the saved network keeps its stored one... */
  CHECK(run("wifi join hidden-net") == AMIGA_SCRIPT_OK);
  CHECK_STR(fake_wifi_password(), "s3cret-passphrase");
  /* ...and another network is joined as open. */
  CHECK(run("wifi join cafe") == AMIGA_SCRIPT_OK);
  CHECK_STR(fake_wifi_password(), "");
  CHECK(run("wifi join office short") == AMIGA_SCRIPT_ERR);
  CHECK(strstr(transcript, "ERR Passphrase must be 8 to 64") != NULL);

  CHECK(run("wifi scan") == AMIGA_SCRIPT_OK);
  CHECK(run("wifi cancel") == AMIGA_SCRIPT_OK);
  CHECK(ctl.page == AMIGA_PAGE_NETWORK);
  CHECK(run("wifi") == AMIGA_SCRIPT_ERR);
  CHECK(run("wifi bogus") == AMIGA_SCRIPT_ERR);
  CHECK(strstr(transcript, "ERR Bad arguments") != NULL);
}

void test_ctl_net(void)
{
  test_refresh();
  test_scan_and_pick();
  test_join();
  test_script();
}
