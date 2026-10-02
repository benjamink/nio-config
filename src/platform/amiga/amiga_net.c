#include "amiga_net.h"
#include "amiga_fmt.h"

#include <string.h>

/* Scan replies are read a page at a time into a buffer the size of the
 * directory-listing payload, so they stay within what the Amiga transport
 * already carries for Browse. */
#define SCAN_REPLY_MAX 420

static uint8_t scan_reply[SCAN_REPLY_MAX];

static void status(amiga_ctl_t *ctl, const char *msg)
{
  config_nio_set_status(ctl->state, msg);
}

/* ---- Pure helpers ------------------------------------------------------- */

const char *amiga_net_link_text(uint8_t link_state)
{
  switch (link_state) {
  case 0: return "Disconnected";
  case 1: return "Connecting";
  case 2: return "Connected";
  case 3: return "Failed to connect";
  default: return "Unknown";
  }
}

const char *amiga_net_signal_text(int8_t rssi)
{
  if (rssi >= -55)
    return "Excellent";
  if (rssi >= -67)
    return "Good";
  if (rssi >= -75)
    return "Fair";
  return "Weak";
}

/* The Wi-Fi service reports 0 for an open network, otherwise secured. */
const char *amiga_net_auth_text(uint8_t auth)
{
  return auth ? "Secured" : "Open";
}

void amiga_net_mac_text(char *out, const uint8_t bytes[6])
{
  static const char hex[] = "0123456789ABCDEF";
  uint8_t i;

  for (i = 0; i < 6; i++) {
    *out++ = hex[bytes[i] >> 4];
    *out++ = hex[bytes[i] & 15];
    if (i < 5)
      *out++ = ':';
  }
  *out = 0;
}

static const char *or_unknown(uint8_t have, const char *value)
{
  return have && value[0] ? value : "Unknown";
}

static const char *control_text(const fn_wifi_status_t *s)
{
  switch (s->backend_kind) {
  case FN_WIFI_BACKEND_ESP32: return "FujiNet (ESP32)";
  case FN_WIFI_BACKEND_POSIX_HOST: return "Host computer";
  case FN_WIFI_BACKEND_POSIX_SIMULATED: return "Simulated";
  default: return s->capabilities ? "Unavailable" : "Unknown";
  }
}

void amiga_net_row_text(const amiga_net_t *net, uint16_t row, char *out)
{
  static const char *const labels[AMIGA_NET_ROWS] = {
    "Wi-Fi", "Network", "Signal", "Access point", "IP address",
    "Subnet mask", "Gateway", "DNS server", "MAC address", "Firmware",
    "Wi-Fi control"
  };
  const fn_wifi_status_t *s = &net->status;
  uint8_t hs = net->have_status;
  uint8_t connected = hs && s->link_state == 2;
  char value[64];

  value[0] = 0;
  switch (row) {
  case AMIGA_NET_ROW_LINK:
    if (!hs)
      strcpy(value, "Unknown");
    else if (net->have_config && !net->config.enabled)
      strcpy(value, "Off");
    else
      strcpy(value, amiga_net_link_text(s->link_state));
    break;
  case AMIGA_NET_ROW_SSID:
    if (!net->have_config)
      strcpy(value, "Unknown");
    else if (!net->config.ssid[0])
      strcpy(value, "(none set)");
    else
      amiga_sprintf(value, "%.40s", net->config.ssid);
    break;
  case AMIGA_NET_ROW_SIGNAL:
    /* 0 dBm is never a real reading: backends without one report it. */
    if (connected && !s->rssi)
      strcpy(value, "Unknown");
    else if (connected)
      amiga_sprintf(value, "%d dBm (%s)", (int) s->rssi,
                    amiga_net_signal_text(s->rssi));
    else
      strcpy(value, "-");
    break;
  case AMIGA_NET_ROW_BSSID:
    if (connected && s->bssid.valid)
      amiga_net_mac_text(value, s->bssid.bytes);
    else
      strcpy(value, "-");
    break;
  case AMIGA_NET_ROW_IP:
    strcpy(value, connected ? or_unknown(hs, s->ip) : "-");
    break;
  case AMIGA_NET_ROW_SUBNET:
    strcpy(value, connected ? or_unknown(hs, s->subnet) : "-");
    break;
  case AMIGA_NET_ROW_GATEWAY:
    strcpy(value, connected ? or_unknown(hs, s->gateway) : "-");
    break;
  case AMIGA_NET_ROW_DNS:
    strcpy(value, connected ? or_unknown(hs, s->dns) : "-");
    break;
  case AMIGA_NET_ROW_MAC:
    if (net->have_adapter && net->adapter.mac.valid)
      amiga_net_mac_text(value, net->adapter.mac.bytes);
    else
      strcpy(value, net->adapter_error == FN_ERR_UNSUPPORTED
                      ? "Needs newer firmware" : "Unknown");
    break;
  case AMIGA_NET_ROW_FIRMWARE:
    if (net->have_adapter && net->adapter.firmware[0])
      amiga_sprintf(value, "%.40s", net->adapter.firmware);
    else
      strcpy(value, net->adapter_error == FN_ERR_UNSUPPORTED
                      ? "Needs newer firmware" : "Unknown");
    break;
  case AMIGA_NET_ROW_CONTROL:
    strcpy(value, hs ? control_text(s) : "Unknown");
    break;
  default:
    out[0] = 0;
    return;
  }
  amiga_sprintf(out, "%-14s%s", labels[row], value);
}

void amiga_net_scan_row_text(const fn_wifi_scan_record_t *r, uint8_t ssid_w,
                             char *out)
{
  char name[FN_WIFI_MAX_SSID + 1];
  uint8_t n;

  strcpy(name, r->ssid);
  n = (uint8_t) strlen(name);
  if (n > ssid_w)
    name[ssid_w] = 0;
  /* amiga_sprintf has no '*' width; pad by hand. */
  memset(out, ' ', ssid_w);
  memcpy(out, name, strlen(name));
  amiga_sprintf(out + ssid_w, " %4d dBm  %-9s %s", (int) r->rssi,
                amiga_net_signal_text(r->rssi), amiga_net_auth_text(r->auth));
}

const char *amiga_net_pass_problem(uint8_t secured, uint16_t len,
                                   uint8_t saved_with_pass)
{
  if (len > AMIGA_NET_PASS_MAX)
    return "Passphrase is longer than 64 characters";
  if (!secured)
    return NULL;
  if (len == 0)
    return saved_with_pass ? NULL : "Type the network's passphrase first";
  if (len < AMIGA_NET_PASS_MIN)
    return "Passphrase must be 8 to 64 characters";
  return NULL;
}

/* ---- Controller --------------------------------------------------------- */

int amiga_ctl_net_refresh(amiga_ctl_t *ctl)
{
  amiga_net_t *n = &ctl->net;
  uint8_t err;

  err = fn_wifi_get_status(&n->status);
  n->have_status = err == FN_OK;
  if (!n->have_status)
    memset(&n->status, 0, sizeof(n->status));
  n->have_config = fn_wifi_get_config(&n->config) == FN_OK;
  if (!n->have_config)
    memset(&n->config, 0, sizeof(n->config));
  n->adapter_error = fn_wifi_get_adapter_info(&n->adapter);
  n->have_adapter = n->adapter_error == FN_OK;
  if (!n->have_adapter)
    memset(&n->adapter, 0, sizeof(n->adapter));
  amiga_list_set_count(&ctl->netinfo, AMIGA_NET_ROWS);

  if (!n->have_status) {
    if (err == FN_ERR_UNSUPPORTED)
      status(ctl, "This FujiNet does not report Wi-Fi status");
    else {
      amiga_sprintf(ctl->msg, "Wi-Fi status failed: error %u", (unsigned) err);
      status(ctl, ctl->msg);
    }
    return 0;
  }
  if (n->have_config && n->config.ssid[0])
    amiga_sprintf(ctl->msg, "%s: %.32s",
                  amiga_net_link_text(n->status.link_state), n->config.ssid);
  else
    strcpy(ctl->msg, amiga_net_link_text(n->status.link_state));
  status(ctl, ctl->msg);
  return 1;
}

uint8_t amiga_ctl_net_poll(amiga_ctl_t *ctl)
{
  if (!amiga_ctl_net_refresh(ctl))
    return 0xFF;
  return ctl->net.status.link_state;
}

static int scan_all(amiga_ctl_t *ctl)
{
  amiga_net_t *n = &ctl->net;
  uint16_t offset = 0;   /* wire record index; hidden ones are not kept */
  uint8_t count;
  uint8_t more = 1;
  uint8_t err;
  uint8_t base;
  uint8_t i;

  n->scan_count = 0;
  amiga_list_set_count(&ctl->networks, 0);
  while (more && n->scan_count < AMIGA_NET_SCAN_MAX) {
    count = 0;
    err = fn_wifi_scan(offset,
                       (uint8_t) (AMIGA_NET_SCAN_MAX - n->scan_count),
                       &n->scan[n->scan_count],
                       (uint8_t) (AMIGA_NET_SCAN_MAX - n->scan_count),
                       &count, &more, scan_reply, sizeof(scan_reply));
    if (err != FN_OK) {
      if (err == FN_ERR_UNSUPPORTED)
        status(ctl, "This FujiNet cannot scan for networks");
      else {
        amiga_sprintf(ctl->msg, "Scan failed: error %u", (unsigned) err);
        status(ctl, ctl->msg);
      }
      n->scan_count = 0;
      return 0;
    }
    if (!count)
      break;
    offset = (uint16_t) (offset + count);
    /* Hidden networks have no name to list; Join Other reaches them.
     * Compact this page in place, behind the networks already kept. */
    base = n->scan_count;
    for (i = 0; i < count; i++) {
      if (n->scan[base + i].ssid[0])
        n->scan[n->scan_count++] = n->scan[base + i];
    }
  }
  amiga_list_set_count(&ctl->networks, n->scan_count);
  if (!n->scan_count) {
    status(ctl, "No networks found; press Rescan to try again");
    return 1;
  }
  amiga_sprintf(ctl->msg, "Found %u network%s; choose one and press Join",
                (unsigned) n->scan_count, n->scan_count == 1 ? "" : "s");
  status(ctl, ctl->msg);
  return 1;
}

/* The saved network when it is in the list, else the strongest. */
static uint16_t preferred_row(amiga_ctl_t *ctl)
{
  amiga_net_t *n = &ctl->net;
  uint16_t best = 0;
  uint16_t i;

  for (i = 0; i < n->scan_count; i++) {
    if (n->have_config && n->config.ssid[0] &&
        strcmp(n->scan[i].ssid, n->config.ssid) == 0)
      return i;
    if (n->scan[i].rssi > n->scan[best].rssi)
      best = i;
  }
  return best;
}

static int can_join(amiga_ctl_t *ctl)
{
  const fn_wifi_status_t *s = &ctl->net.status;

  if (ctl->net.have_status && s->capabilities &&
      !(s->capabilities & FN_WIFI_CAP_CONNECT)) {
    status(ctl, (s->capabilities & FN_WIFI_CAP_HOST)
                  ? "Wi-Fi is managed by the host computer"
                  : "This FujiNet cannot change networks");
    return 0;
  }
  return 1;
}

int amiga_ctl_wifi_begin(amiga_ctl_t *ctl)
{
  if (!ctl->net.have_status)
    (void) amiga_ctl_net_refresh(ctl);
  if (!can_join(ctl))
    return 0;
  if (!scan_all(ctl))
    return 0;
  if (ctl->page != AMIGA_PAGE_WIFI)
    ctl->net.net_return = ctl->page;
  ctl->page = AMIGA_PAGE_WIFI;
  ctl->networks.top = 0;
  if (ctl->net.scan_count)
    amiga_list_select(&ctl->networks, preferred_row(ctl));
  return 1;
}

int amiga_ctl_wifi_rescan(amiga_ctl_t *ctl)
{
  int ok = scan_all(ctl);

  ctl->networks.top = 0;
  if (ctl->net.scan_count)
    amiga_list_select(&ctl->networks, preferred_row(ctl));
  return ok;
}

void amiga_ctl_wifi_cancel(amiga_ctl_t *ctl)
{
  if (ctl->page == AMIGA_PAGE_WIFI)
    ctl->page = ctl->net.net_return;
}

static int is_saved_with_pass(amiga_ctl_t *ctl, const char *ssid)
{
  const fn_wifi_config_t *c = &ctl->net.config;

  return ctl->net.have_config && c->password_present &&
         strcmp(c->ssid, ssid) == 0;
}

int amiga_ctl_wifi_is_saved(amiga_ctl_t *ctl, uint16_t index)
{
  return index < ctl->net.scan_count &&
         is_saved_with_pass(ctl, ctl->net.scan[index].ssid);
}

int amiga_ctl_wifi_needs_pass(amiga_ctl_t *ctl, uint16_t index)
{
  const fn_wifi_scan_record_t *r;

  if (index >= ctl->net.scan_count)
    return 0;
  r = &ctl->net.scan[index];
  return r->auth && !is_saved_with_pass(ctl, r->ssid);
}

int amiga_ctl_wifi_join(amiga_ctl_t *ctl, const char *ssid, const char *pass,
                        uint8_t secured)
{
  fn_wifi_config_update_t u;
  size_t plen = pass ? strlen(pass) : 0;
  uint8_t err;

  if (!ssid || !ssid[0]) {
    status(ctl, "Choose a network first");
    return 0;
  }
  if (strlen(ssid) > FN_WIFI_MAX_SSID) {
    status(ctl, "Network name is too long");
    return 0;
  }
  {
    const char *problem = amiga_net_pass_problem(secured, (uint16_t) plen,
                                             (uint8_t) is_saved_with_pass(ctl, ssid));

    if (problem) {
      status(ctl, problem);
      return 0;
    }
  }
  if (!can_join(ctl))
    return 0;

  memset(&u, 0, sizeof(u));
  /* An empty BSSID lets the FujiNet use any access point of the network,
   * replacing a pin left from a previous one. */
  u.fields = FN_WIFI_SET_ENABLED | FN_WIFI_SET_SSID | FN_WIFI_SET_BSSID |
             FN_WIFI_SET_PERSIST | FN_WIFI_SET_RECONNECT;
  u.enabled = 1;
  u.ssid = ssid;
  u.bssid = "";
  /* Empty: keep the stored passphrase (rejoining the saved network).
   * An open network is joined with no passphrase. */
  if (plen || !secured) {
    u.fields |= FN_WIFI_SET_PASSWORD;
    u.password = secured ? pass : "";
  }
  err = fn_wifi_set_config(&u);
  if (err != FN_OK) {
    if (err == FN_ERR_UNSUPPORTED)
      status(ctl, "This FujiNet cannot change networks");
    else if (err == FN_ERR_NOT_READY)
      status(ctl, "The FujiNet could not save the network");
    else {
      amiga_sprintf(ctl->msg, "Joining failed: error %u", (unsigned) err);
      status(ctl, ctl->msg);
    }
    return 0;
  }
  /* Saved; the FujiNet now reconnects on its own schedule. */
  (void) amiga_ctl_net_refresh(ctl);
  amiga_sprintf(ctl->msg, "Saved %.32s; the FujiNet is connecting", ssid);
  status(ctl, ctl->msg);
  return 1;
}

int amiga_ctl_wifi_commit(amiga_ctl_t *ctl, uint16_t index, const char *pass)
{
  const fn_wifi_scan_record_t *r;

  if (index >= ctl->net.scan_count) {
    status(ctl, "Choose a network first");
    return 0;
  }
  r = &ctl->net.scan[index];
  if (!r->ssid[0]) {
    status(ctl, "Hidden networks need their name; use Join Other");
    return 0;
  }
  if (!amiga_ctl_wifi_join(ctl, r->ssid, pass, r->auth))
    return 0;
  ctl->page = ctl->net.net_return;
  return 1;
}

void amiga_ctl_wifi_hint(amiga_ctl_t *ctl, uint16_t index)
{
  const fn_wifi_scan_record_t *r;

  if (index >= ctl->net.scan_count)
    return;
  r = &ctl->net.scan[index];
  if (!r->auth)
    status(ctl, "Open network: press Join");
  else if (!amiga_ctl_wifi_needs_pass(ctl, index))
    status(ctl, "Saved network: press Join to reconnect");
  else
    status(ctl, "Secured network: press Join to enter the passphrase");
}

uint8_t amiga_ctl_net_join_result(amiga_ctl_t *ctl, const char *ssid)
{
  uint8_t link = amiga_ctl_net_poll(ctl);

  if (link == 2)
    amiga_sprintf(ctl->msg, "Connected to %.32s, address %s", ssid,
                  ctl->net.status.ip[0] ? ctl->net.status.ip : "pending");
  else if (link == 3)
    amiga_sprintf(ctl->msg, "Could not connect to %.32s; check the passphrase",
                  ssid);
  else if (link != 0xFF)
    amiga_sprintf(ctl->msg, "Still connecting to %.32s; press Refresh", ssid);
  else
    return link;   /* keep the read failure message */
  status(ctl, ctl->msg);
  return link;
}
