#ifndef AMIGA_TEST_FAKE_NIO_H
#define AMIGA_TEST_FAKE_NIO_H

#include <stdint.h>

#include "fujinet-nio.h"

typedef struct {
  uint8_t is_dir;
  const char *name;
  uint32_t size;
  uint32_t mtime;
} fake_dir_entry_t;

void fake_nio_reset(void);
void fake_appstore_put(const char *ns, const char *key, const void *data,
                       uint16_t len);
const uint8_t *fake_appstore_get(const char *ns, const char *key,
                                 uint16_t *len);
void fake_slot_put(uint8_t index, const char *uri, uint8_t readonly);
const char *fake_slot_uri(uint8_t index);
uint8_t fake_slot_readonly(uint8_t index);
unsigned fake_slot_get_calls(void);
unsigned fake_slot_range_calls(void);
void fake_dir_put(const char *uri, const fake_dir_entry_t *entries,
                  uint8_t count);
void fake_dir_fail(const char *uri);

/* Wi-Fi service.  Reset leaves a connected ESP32-style adapter on "home"
 * with a stored passphrase, firmware "0.1.1" and no scan results. */
fn_wifi_status_t *fake_wifi_status(void);
fn_wifi_config_t *fake_wifi_config(void);
void fake_wifi_status_error(uint8_t err);    /* FN_OK: answer normally */
void fake_wifi_adapter_error(uint8_t err);   /* e.g. FN_ERR_UNSUPPORTED */
void fake_wifi_set_error(uint8_t err);
void fake_wifi_scan_error(uint8_t err);
void fake_wifi_add_network(const char *ssid, int8_t rssi, uint8_t auth);
unsigned fake_wifi_set_calls(void);
unsigned fake_wifi_scan_calls(void);
uint8_t fake_wifi_last_fields(void);
const char *fake_wifi_password(void);        /* stored passphrase */
const char *fake_wifi_bssid(void);           /* stored BSSID */

#endif
