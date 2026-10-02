#include "check.h"
#include "amiga_net.h"

void test_net(void)
{
  static const uint8_t mac[6] = { 0x24, 0x6F, 0x28, 0x0A, 0xBC, 0x01 };
  amiga_net_t net;
  fn_wifi_scan_record_t r;
  char text[96];

  amiga_net_mac_text(text, mac);
  CHECK_STR(text, "24:6F:28:0A:BC:01");

  CHECK_STR(amiga_net_link_text(0), "Disconnected");
  CHECK_STR(amiga_net_link_text(2), "Connected");
  CHECK_STR(amiga_net_link_text(3), "Failed to connect");
  CHECK_STR(amiga_net_link_text(9), "Unknown");
  CHECK_STR(amiga_net_signal_text(-40), "Excellent");
  CHECK_STR(amiga_net_signal_text(-67), "Good");
  CHECK_STR(amiga_net_signal_text(-70), "Fair");
  CHECK_STR(amiga_net_signal_text(-90), "Weak");
  CHECK_STR(amiga_net_auth_text(0), "Open");
  CHECK_STR(amiga_net_auth_text(1), "Secured");

  /* Signal icon levels and the rows that leave room for it. */
  CHECK(amiga_net_signal_bars(-40) == 4 && amiga_net_signal_bars(-55) == 4);
  CHECK(amiga_net_signal_bars(-56) == 3 && amiga_net_signal_bars(-67) == 3);
  CHECK(amiga_net_signal_bars(-70) == 2 && amiga_net_signal_bars(-80) == 1);
  CHECK(amiga_net_signal_bars(-90) == 0);
  {
    amiga_net_t n;
    fn_wifi_scan_record_t sr;
    uint8_t col = 0;

    memset(&n, 0, sizeof(n));
    n.have_status = 1;
    n.status.link_state = 2;
    n.status.rssi = -61;
    CHECK(amiga_net_row_display(&n, AMIGA_NET_ROW_SIGNAL, text, &col) == 3);
    CHECK(col == AMIGA_NET_LABEL_W);
    CHECK_STR(text, "Signal            Good");
    /* Other rows, and a missing reading, have no icon. */
    CHECK(amiga_net_row_display(&n, AMIGA_NET_ROW_IP, text, &col) == -1);
    n.status.rssi = 0;
    CHECK(amiga_net_row_display(&n, AMIGA_NET_ROW_SIGNAL, text, &col) == -1);
    CHECK_STR(text, "Signal        Unknown");
    n.status.link_state = 0;
    n.status.rssi = -50;
    CHECK(amiga_net_row_display(&n, AMIGA_NET_ROW_SIGNAL, text, &col) == -1);
    CHECK_STR(text, "Signal        -");

    memset(&sr, 0, sizeof(sr));
    strcpy(sr.ssid, "home");
    sr.rssi = -48;
    sr.auth = 1;
    CHECK(amiga_net_scan_row_display(&sr, 10, text, &col) == 4);
    CHECK(col == 11);
    CHECK_STR(text, "home            Excellent Secured");
    strcpy(sr.ssid, "a-very-long-network-name");
    sr.rssi = -90;
    sr.auth = 0;
    CHECK(amiga_net_scan_row_display(&sr, 6, text, &col) == 0);
    CHECK_STR(text, "a-very      Weak      Open");
  }

  /* Join window validation, shared with the controller and SCRIPT. */
  CHECK(amiga_net_pass_problem(0, 0, 0) == NULL);                 /* open */
  CHECK(amiga_net_pass_problem(1, 12, 0) == NULL);
  CHECK(amiga_net_pass_problem(1, 0, 1) == NULL);                 /* keep saved */
  CHECK_STR(amiga_net_pass_problem(1, 0, 0), "Type the network's passphrase first");
  CHECK_STR(amiga_net_pass_problem(1, 5, 1), "Passphrase must be 8 to 64 characters");
  CHECK_STR(amiga_net_pass_problem(1, 65, 0), "Passphrase is longer than 64 characters");

  /* Nothing read yet: every row says so rather than showing zeroes. */
  memset(&net, 0, sizeof(net));
  amiga_net_row_text(&net, AMIGA_NET_ROW_LINK, text);
  CHECK_STR(text, "Wi-Fi         Unknown");
  amiga_net_row_text(&net, AMIGA_NET_ROW_MAC, text);
  CHECK_STR(text, "MAC address   Unknown");
  amiga_net_row_text(&net, AMIGA_NET_ROW_IP, text);
  CHECK_STR(text, "IP address    -");

  /* Connected: addresses, signal and adapter details. */
  net.have_status = net.have_config = net.have_adapter = 1;
  net.status.link_state = 2;
  net.status.rssi = -61;
  net.status.bssid.valid = 1;
  net.status.bssid.bytes[0] = 0x02;
  net.status.backend_kind = FN_WIFI_BACKEND_ESP32;
  strcpy(net.status.ip, "192.168.1.50");
  strcpy(net.status.gateway, "192.168.1.1");
  net.config.enabled = 1;
  strcpy(net.config.ssid, "home");
  memcpy(net.adapter.mac.bytes, mac, 6);
  net.adapter.mac.valid = 1;
  strcpy(net.adapter.firmware, "0.1.1");
  amiga_net_row_text(&net, AMIGA_NET_ROW_LINK, text);
  CHECK_STR(text, "Wi-Fi         Connected");
  amiga_net_row_text(&net, AMIGA_NET_ROW_SSID, text);
  CHECK_STR(text, "Network       home");
  amiga_net_row_text(&net, AMIGA_NET_ROW_SIGNAL, text);
  CHECK_STR(text, "Signal        -61 dBm (Good)");
  amiga_net_row_text(&net, AMIGA_NET_ROW_BSSID, text);
  CHECK_STR(text, "Access point  02:00:00:00:00:00");
  amiga_net_row_text(&net, AMIGA_NET_ROW_IP, text);
  CHECK_STR(text, "IP address    192.168.1.50");
  amiga_net_row_text(&net, AMIGA_NET_ROW_SUBNET, text);
  CHECK_STR(text, "Subnet mask   Unknown");
  amiga_net_row_text(&net, AMIGA_NET_ROW_MAC, text);
  CHECK_STR(text, "MAC address   24:6F:28:0A:BC:01");
  amiga_net_row_text(&net, AMIGA_NET_ROW_FIRMWARE, text);
  CHECK_STR(text, "Firmware      0.1.1");
  amiga_net_row_text(&net, AMIGA_NET_ROW_CONTROL, text);
  CHECK_STR(text, "Wi-Fi control FujiNet (ESP32)");

  net.status.rssi = 0;
  amiga_net_row_text(&net, AMIGA_NET_ROW_SIGNAL, text);
  CHECK_STR(text, "Signal        Unknown");

  /* Not connected: no stale addresses; Wi-Fi switched off says so. */
  net.status.link_state = 0;
  amiga_net_row_text(&net, AMIGA_NET_ROW_IP, text);
  CHECK_STR(text, "IP address    -");
  amiga_net_row_text(&net, AMIGA_NET_ROW_SIGNAL, text);
  CHECK_STR(text, "Signal        -");
  net.config.enabled = 0;
  amiga_net_row_text(&net, AMIGA_NET_ROW_LINK, text);
  CHECK_STR(text, "Wi-Fi         Off");
  net.config.ssid[0] = 0;
  amiga_net_row_text(&net, AMIGA_NET_ROW_SSID, text);
  CHECK_STR(text, "Network       (none set)");

  /* Older firmware without adapter details. */
  net.have_adapter = 0;
  net.adapter_error = FN_ERR_UNSUPPORTED;
  amiga_net_row_text(&net, AMIGA_NET_ROW_FIRMWARE, text);
  CHECK_STR(text, "Firmware      Needs newer firmware");

  /* Join picker rows: the name padded or cut to its column. */
  memset(&r, 0, sizeof(r));
  strcpy(r.ssid, "home");
  r.rssi = -48;
  r.auth = 1;
  amiga_net_scan_row_text(&r, 10, text);
  CHECK_STR(text, "home        -48 dBm  Excellent Secured");
  strcpy(r.ssid, "a-very-long-network-name");
  r.rssi = -80;
  r.auth = 0;
  amiga_net_scan_row_text(&r, 6, text);
  CHECK_STR(text, "a-very  -80 dBm  Weak      Open");
}
