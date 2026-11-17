#include "wifi_trs80.h"
#include "fnctl.h"

#include <string.h>

#define DEV_WIFI 0xF3
#define PROTO 1
#define CMD_GET_STATUS 0x01
#define CMD_SCAN 0x04
#define CMD_TRY_JOIN 0x06
#define CMD_JOIN_STATUS 0x07

static uint8_t req[3 + WIFI_SSID_MAX + 64];
static uint8_t reply[WIFI_SCAN_MAX * 44 + 8];

static int call(uint8_t cmd, uint16_t req_len, uint16_t *len)
{
  uint8_t status;

  if (!fnctl_nio_call(DEV_WIFI, cmd, req, req_len, reply, sizeof(reply), &status, len))
    return 0;
  return status == 0 && *len >= 1 && reply[0] == PROTO;
}

int wifi_status(uint8_t *link_state, char *ip, uint8_t ip_cap)
{
  uint16_t len;
  uint8_t n;

  req[0] = PROTO;
  if (!call(CMD_GET_STATUS, 1, &len) || len < 13)
    return 0;
  *link_state = reply[1];
  n = reply[12];
  if (12 + 1 + n > len || n >= ip_cap)
    return 0;
  memcpy(ip, &reply[13], n);
  ip[n] = 0;
  return 1;
}

int wifi_scan(uint16_t offset, wifi_net_t *nets, uint8_t *count, uint8_t *more)
{
  uint16_t len, at;
  uint8_t i, n, sl;

  req[0] = PROTO;
  req[1] = (uint8_t) offset;
  req[2] = (uint8_t) (offset >> 8);
  req[3] = WIFI_SCAN_MAX;
  if (!call(CMD_SCAN, 4, &len) || len < 3)
    return 0;
  *more = reply[1];
  n = reply[2];
  if (n > WIFI_SCAN_MAX)
    return 0;
  at = 3;
  for (i = 0; i < n; i++) {
    if (at >= len)
      return 0;
    sl = reply[at++];
    if (sl > WIFI_SSID_MAX || (uint16_t) (at + sl + 9) > len)
      return 0;
    memcpy(nets[i].ssid, &reply[at], sl);
    nets[i].ssid[sl] = 0;
    at += sl + 6;                         /* bssid */
    nets[i].rssi = (int8_t) reply[at++];
    at++;                                 /* channel */
    nets[i].auth = reply[at++];
  }
  *count = n;
  return 1;
}

int wifi_join(const char *ssid, const char *password)
{
  uint16_t len, at;
  uint8_t sl = (uint8_t) strlen(ssid), pl = (uint8_t) strlen(password);

  if (!sl || sl > WIFI_SSID_MAX || pl > 64)
    return 0;
  req[0] = PROTO;
  req[1] = sl;
  memcpy(&req[2], ssid, sl);
  at = (uint16_t) (2 + sl);
  req[at++] = pl;
  memcpy(&req[at], password, pl);
  at += pl;
  return call(CMD_TRY_JOIN, at, &len);
}

int wifi_join_state(uint8_t *state)
{
  uint16_t len;

  req[0] = PROTO;
  if (!call(CMD_JOIN_STATUS, 1, &len) || len < 2 || reply[1] > 3)
    return 0;
  *state = reply[1];
  return 1;
}
