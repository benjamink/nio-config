#ifndef TRS80_WIFI_H
#define TRS80_WIFI_H

#include <stdint.h>

/* Wi-Fi device (0xF3) client for the 4P. fujinet-nio-lib's fn_wifi.c does not compile under sccz80,
 * so these speak the same wire protocol through fnctl_nio_call. All return 1 on success. */
#define WIFI_SSID_MAX 32
#define WIFI_SCAN_MAX 6

typedef struct {
  char ssid[WIFI_SSID_MAX + 1];
  int8_t rssi;
  uint8_t auth;
} wifi_net_t;

int wifi_status(uint8_t *link_state, char *ip, uint8_t ip_cap);
/* Fills nets[0..*count); *more is set when the adapter has more networks. */
int wifi_scan(uint16_t offset, wifi_net_t *nets, uint8_t *count, uint8_t *more);
int wifi_join(const char *ssid, const char *password);
int wifi_join_state(uint8_t *state);          /* 0 idle, 1 pending, 2 connected, 3 failed */

#endif
