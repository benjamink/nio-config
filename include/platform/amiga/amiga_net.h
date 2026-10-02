#ifndef AMIGA_NET_H
#define AMIGA_NET_H

#include "amiga_ctl.h"

/* Network page: Wi-Fi status, addresses, MAC and firmware version, read
 * through the fujinet-nio Wi-Fi service.  Join scans for networks, takes a
 * passphrase and saves the choice on the FujiNet, which then reconnects. */

#define AMIGA_NET_LABEL_W 14

/* Configuration window rows: the Network tab shows the first
 * AMIGA_NET_NETWORK_ROWS, the Device tab AMIGA_NET_ROW_FIRMWARE. */
enum {
  AMIGA_NET_ROW_LINK = 0,
  AMIGA_NET_ROW_SSID,
  AMIGA_NET_ROW_SIGNAL,
  AMIGA_NET_ROW_BSSID,
  AMIGA_NET_ROW_IP,
  AMIGA_NET_ROW_SUBNET,
  AMIGA_NET_ROW_GATEWAY,
  AMIGA_NET_ROW_DNS,
  AMIGA_NET_ROW_MAC,
  AMIGA_NET_ROW_CONTROL,
  AMIGA_NET_ROW_FIRMWARE
};
#define AMIGA_NET_NETWORK_ROWS AMIGA_NET_ROW_FIRMWARE

#define AMIGA_NET_PASS_MIN 8
#define AMIGA_NET_PASS_MAX FN_WIFI_MAX_PASSWORD

/* Pure helpers (no FujiNet calls). */
/* Why a passphrase of `len` characters cannot be used to join, or NULL.
 * Empty is allowed for an open network and for the saved network whose
 * passphrase is stored (it is kept). */
const char *amiga_net_pass_problem(uint8_t secured, uint16_t len,
                                   uint8_t saved_with_pass);
const char *amiga_net_link_text(uint8_t link_state);
const char *amiga_net_signal_text(int8_t rssi);
const char *amiga_net_auth_text(uint8_t auth);
void amiga_net_mac_text(char *out, const uint8_t bytes[6]);
/* "Label         value" for a Network page row; out holds 80 chars. */
void amiga_net_row_text(const amiga_net_t *net, uint16_t row, char *out);
/* Join picker row: SSID padded to ssid_w, then signal, quality, security. */
void amiga_net_scan_row_text(const fn_wifi_scan_record_t *r, uint8_t ssid_w,
                             char *out);

/* Re-reads status, saved config and adapter details.  Succeeds when the
 * status could be read; MAC and firmware are optional. */
int amiga_ctl_net_refresh(amiga_ctl_t *ctl);
/* Link state after re-reading the status, or 0xFF when that failed. */
uint8_t amiga_ctl_net_poll(amiga_ctl_t *ctl);

/* Join picker: scan (all pages, at most AMIGA_NET_SCAN_MAX networks) and
 * show the list with the joined network, else the strongest, selected. */
int amiga_ctl_wifi_begin(amiga_ctl_t *ctl);
int amiga_ctl_wifi_rescan(amiga_ctl_t *ctl);
void amiga_ctl_wifi_cancel(amiga_ctl_t *ctl);
/* Whether joining scan row `index` needs a passphrase typed: a secured
 * network that is not the saved one with a stored passphrase. */
int amiga_ctl_wifi_needs_pass(amiga_ctl_t *ctl, uint16_t index);
/* Whether scan row `index` is the saved network with a stored passphrase
 * (Join may then keep it). */
int amiga_ctl_wifi_is_saved(amiga_ctl_t *ctl, uint16_t index);
/* Status line for the selected scan row: what Join will need. */
void amiga_ctl_wifi_hint(amiga_ctl_t *ctl, uint16_t index);
/* After a join and a wait: re-reads the status and reports whether the
 * FujiNet connected to `ssid`, failed, or is still connecting.  Returns
 * the link state, or 0xFF when the status could not be read. */
uint8_t amiga_ctl_net_join_result(amiga_ctl_t *ctl, const char *ssid);
/* Joins scan row `index` and returns to the Network page. */
int amiga_ctl_wifi_commit(amiga_ctl_t *ctl, uint16_t index, const char *pass);
/* Saves `ssid` (any name, e.g. a hidden network) with `pass` and asks the
 * FujiNet to reconnect.  An empty pass keeps the stored passphrase when
 * ssid is the saved network. */
int amiga_ctl_wifi_join(amiga_ctl_t *ctl, const char *ssid, const char *pass,
                        uint8_t secured);

#endif
