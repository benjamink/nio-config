/* In-memory stand-in for the fujinet-nio calls used by config_nio_store.c,
 * config_nio_state.c and amiga_ctl.c.  Behaviour follows the documented
 * contracts in fujinet-nio.h (missing keys: FN_OK, EXISTS clear, EOF set). */
#include "fake_nio.h"
#include "fnctl.h"
#include "fnsvc.h"
#include "fujinet-nio.h"

#include <string.h>

#define FAKE_KEYS 16
#define FAKE_VALUE_MAX 4096
#define FAKE_DIRS 8

typedef struct {
  uint8_t used;
  char ns[24];
  char key[24];
  uint16_t len;
  uint8_t data[FAKE_VALUE_MAX];
} fake_key_t;

typedef struct {
  uint8_t used;
  uint8_t readonly;
  char uri[FNSVC_MAX_URI + 1];
} fake_slot_t;

typedef struct {
  uint8_t used;
  uint8_t fail;
  char uri[FNSVC_MAX_URI + 1];
  const fake_dir_entry_t *entries;
  uint8_t count;
} fake_dir_t;

static fake_key_t keys[FAKE_KEYS];
static fake_slot_t slots[256];
static fake_dir_t dirs[FAKE_DIRS];
static unsigned slot_get_calls;
static unsigned slot_range_calls;
static uint8_t range_buf[1024];
static uint8_t last_error;
static uint8_t last_status;

static void fake_wifi_reset(void);

void fake_nio_reset(void)
{
  memset(keys, 0, sizeof(keys));
  memset(slots, 0, sizeof(slots));
  memset(dirs, 0, sizeof(dirs));
  slot_get_calls = 0;
  slot_range_calls = 0;
  last_error = 0;
  last_status = 0;
  fake_wifi_reset();
}

static fake_key_t *find_key(const char *ns, const char *key, int create)
{
  int i;
  fake_key_t *free_key = NULL;

  for (i = 0; i < FAKE_KEYS; i++) {
    if (keys[i].used && !strcmp(keys[i].ns, ns) && !strcmp(keys[i].key, key))
      return &keys[i];
    if (!keys[i].used && !free_key)
      free_key = &keys[i];
  }
  if (!create || !free_key)
    return NULL;
  memset(free_key, 0, sizeof(*free_key));
  free_key->used = 1;
  strcpy(free_key->ns, ns);
  strcpy(free_key->key, key);
  return free_key;
}

void fake_appstore_put(const char *ns, const char *key, const void *data,
                       uint16_t len)
{
  fake_key_t *k = find_key(ns, key, 1);

  memcpy(k->data, data, len);
  k->len = len;
}

const uint8_t *fake_appstore_get(const char *ns, const char *key,
                                 uint16_t *len)
{
  fake_key_t *k = find_key(ns, key, 0);

  if (!k)
    return NULL;
  *len = k->len;
  return k->data;
}

uint8_t fn_appstore_read(fn_appstore_io_t *io, const char *ns,
                         const char *key, uint32_t offset, uint8_t *buf,
                         uint16_t max_len, fn_appstore_read_t *out)
{
  fake_key_t *k;
  uint16_t remaining;

  (void) io;
  memset(out, 0, sizeof(*out));
  out->offset = offset;
  k = find_key(ns, key, 0);
  if (!k) {
    out->flags = FN_APPSTORE_READ_EOF;
    return FN_OK;
  }
  out->flags = FN_APPSTORE_READ_EXISTS;
  remaining = offset >= k->len ? 0 : (uint16_t) (k->len - offset);
  if (remaining > max_len)
    remaining = max_len;
  else
    out->flags |= FN_APPSTORE_READ_EOF;
  memcpy(buf, k->data + offset, remaining);
  out->bytes_read = remaining;
  return FN_OK;
}

uint8_t fn_appstore_write(fn_appstore_io_t *io, const char *ns,
                          const char *key, uint32_t offset,
                          const uint8_t *data, uint16_t len,
                          fn_appstore_write_t *out)
{
  fake_key_t *k;

  (void) io;
  k = find_key(ns, key, 1);
  if (!k || offset + len > FAKE_VALUE_MAX)
    return FN_ERR_IO;
  if (offset == 0)
    k->len = 0;
  memcpy(k->data + offset, data, len);
  if (offset + len > k->len)
    k->len = (uint16_t) (offset + len);
  out->offset = offset;
  out->bytes_written = len;
  return FN_OK;
}

uint8_t fn_appstore_delete(fn_appstore_io_t *io, const char *ns,
                           const char *key, fn_appstore_delete_t *out)
{
  fake_key_t *k;

  (void) io;
  k = find_key(ns, key, 0);
  out->deleted = k != NULL;
  if (k)
    k->used = 0;
  return FN_OK;
}

void fake_slot_put(uint8_t index, const char *uri, uint8_t readonly)
{
  slots[index].used = 1;
  slots[index].readonly = readonly;
  strcpy(slots[index].uri, uri);
}

const char *fake_slot_uri(uint8_t index)
{
  return slots[index].used ? slots[index].uri : NULL;
}

uint8_t fake_slot_readonly(uint8_t index)
{
  return slots[index].readonly;
}

unsigned fake_slot_get_calls(void)
{
  return slot_get_calls;
}

static void fill_entry(uint8_t index, fn_slot_catalog_entry_t *out)
{
  out->index = index;
  out->flags = (uint8_t) (FN_SLOT_CATALOG_ENTRY_VALID |
                          (slots[index].readonly
                             ? FN_SLOT_CATALOG_ENTRY_READ_ONLY : 0));
  out->uri_len = (uint16_t) strlen(slots[index].uri);
  out->uri = (const uint8_t *) slots[index].uri;
}

uint8_t fn_slot_catalog_get(fn_slot_catalog_io_t *io, uint8_t index,
                            fn_slot_catalog_entry_t *out)
{
  (void) io;
  slot_get_calls++;
  if (!slots[index].used)
    return FN_ERR_NOT_FOUND;
  fill_entry(index, out);
  return FN_OK;
}

uint8_t fn_slot_catalog_put(fn_slot_catalog_io_t *io, uint8_t index,
                            uint8_t flags, const char *target,
                            fn_slot_catalog_entry_t *out)
{
  (void) io;
  fake_slot_put(index, target,
                (uint8_t) ((flags & FN_SLOT_CATALOG_ENTRY_READ_ONLY) != 0));
  fill_entry(index, out);
  return FN_OK;
}

uint8_t fn_slot_catalog_delete(fn_slot_catalog_io_t *io, uint8_t index,
                               uint8_t *deleted)
{
  (void) io;
  *deleted = slots[index].used;
  memset(&slots[index], 0, sizeof(slots[index]));
  return FN_OK;
}

static fake_dir_t *find_dir(const char *uri, int create)
{
  int i;

  for (i = 0; i < FAKE_DIRS; i++) {
    if (dirs[i].used && !strcmp(dirs[i].uri, uri))
      return &dirs[i];
  }
  if (!create)
    return NULL;
  for (i = 0; i < FAKE_DIRS; i++) {
    if (!dirs[i].used) {
      dirs[i].used = 1;
      strcpy(dirs[i].uri, uri);
      return &dirs[i];
    }
  }
  return NULL;
}

void fake_dir_put(const char *uri, const fake_dir_entry_t *entries,
                  uint8_t count)
{
  fake_dir_t *d = find_dir(uri, 1);

  d->fail = 0;
  d->entries = entries;
  d->count = count;
}

void fake_dir_fail(const char *uri)
{
  fake_dir_t *d = find_dir(uri, 1);

  d->fail = 1;
  d->entries = NULL;
  d->count = 0;
}

int fnsvc_list_directory(const char *uri, fnsvc_list_cb cb, void *ctx)
{
  fake_dir_t *d = find_dir(uri, 0);
  uint8_t e;

  if (!d || d->fail) {
    last_error = FNSVC_ERR_STATUS;
    last_status = 5;
    return 0;
  }
  for (e = 0; e < d->count; e++)
    cb(d->entries[e].is_dir, d->entries[e].name, d->entries[e].size,
       d->entries[e].mtime, ctx);
  last_error = FNSVC_ERR_NONE;
  last_status = 0;
  return 1;
}

uint8_t fnsvc_last_error(void)
{
  return last_error;
}

uint8_t fnsvc_last_status(void)
{
  return last_status;
}

int fnsvc_disk_mount(uint8_t slot, const char *uri, uint8_t readonly,
                     uint16_t sector_size_hint)
{
  (void) slot;
  (void) uri;
  (void) readonly;
  (void) sector_size_hint;
  return 1;
}

uint8_t fnsvc_disk_last_error(void)
{
  return FN_DISK_ERR_NONE;
}

int fnctl_set_unit_slot(uint8_t unit, uint8_t slot)
{
  (void) unit;
  (void) slot;
  return 1;
}

unsigned fake_slot_range_calls(void)
{
  return slot_range_calls;
}

/* Pages of at most five entries, in the library's [slot, flags, len, uri]
 * entry format, so callers must follow next_index/MORE. */
uint8_t fn_slot_catalog_range(fn_slot_catalog_io_t *io, uint8_t lower,
                              uint8_t upper, uint8_t cursor,
                              uint8_t request_flags, uint8_t max_uri_bytes,
                              uint16_t max_payload_bytes,
                              fn_slot_catalog_page_t *out)
{
  uint16_t pos = 0;
  uint16_t i;
  uint8_t count = 0;

  (void) io;
  (void) max_payload_bytes;
  slot_range_calls++;
  if (lower > upper || cursor < lower || cursor > upper || !max_uri_bytes)
    return FN_ERR_INVALID;
  memset(out, 0, sizeof(*out));
  for (i = cursor; i <= upper; i++) {
    const char *uri;
    uint16_t len;
    uint8_t flags;

    if (!slots[i].used)
      continue;
    if (count == 5) {
      out->flags = FN_SLOT_CATALOG_MORE;
      out->next_index = (uint8_t) i;
      break;
    }
    uri = slots[i].uri;
    len = (uint16_t) strlen(uri);
    flags = (uint8_t) (FN_SLOT_CATALOG_ENTRY_VALID |
                       (slots[i].readonly ? FN_SLOT_CATALOG_ENTRY_READ_ONLY : 0));
    if (len > max_uri_bytes) {
      flags |= FN_SLOT_CATALOG_ENTRY_URI_TRUNCATED;
      if (request_flags & FN_SLOT_CATALOG_TAIL_URI)
        uri += len - max_uri_bytes;
      len = max_uri_bytes;
    }
    range_buf[pos++] = (uint8_t) i;
    range_buf[pos++] = flags;
    range_buf[pos++] = (uint8_t) len;
    memcpy(range_buf + pos, uri, len);
    pos = (uint16_t) (pos + len);
    count++;
  }
  out->entry_count = count;
  out->entry_data_len = pos;
  out->entry_data = range_buf;
  return FN_OK;
}

uint8_t fn_slot_catalog_next_entry(const fn_slot_catalog_page_t *page,
                                   uint16_t *offset,
                                   fn_slot_catalog_entry_t *out)
{
  uint16_t pos = *offset;

  if ((uint16_t) (pos + 3) > page->entry_data_len)
    return FN_ERR_IO;
  out->index = page->entry_data[pos];
  out->flags = page->entry_data[pos + 1];
  out->uri_len = page->entry_data[pos + 2];
  out->uri = &page->entry_data[pos + 3];
  *offset = (uint16_t) (pos + 3 + out->uri_len);
  return FN_OK;
}

/* ---- Wi-Fi service ------------------------------------------------------ */

#define FAKE_WIFI_NETS 40
/* Records per scan reply, as the library gets from a 420-byte buffer. */
#define FAKE_WIFI_PAGE 9

static fn_wifi_status_t wifi_status;
static fn_wifi_config_t wifi_config;
static char wifi_password[FN_WIFI_MAX_PASSWORD + 1];
static uint8_t wifi_status_err, wifi_adapter_err, wifi_set_err, wifi_scan_err;
static fn_wifi_scan_record_t wifi_nets[FAKE_WIFI_NETS];
static uint8_t wifi_net_count;
static unsigned wifi_set_calls, wifi_scan_calls;
static uint8_t wifi_last_fields;

static void fake_wifi_reset(void)
{
  memset(&wifi_status, 0, sizeof(wifi_status));
  memset(&wifi_config, 0, sizeof(wifi_config));
  wifi_status.link_state = 2;
  wifi_status.configured_enabled = 1;
  wifi_status.bssid_valid = 1;
  wifi_status.bssid.valid = 1;
  wifi_status.bssid.bytes[0] = 0x02;
  wifi_status.bssid.bytes[5] = 0x01;
  wifi_status.scan_supported = 1;
  wifi_status.rssi = -61;
  wifi_status.capabilities = FN_WIFI_CAP_CONFIG | FN_WIFI_CAP_STATUS |
                             FN_WIFI_CAP_CONNECT | FN_WIFI_CAP_DISCONNECT |
                             FN_WIFI_CAP_SCAN | FN_WIFI_CAP_BSSID;
  wifi_status.backend_kind = FN_WIFI_BACKEND_ESP32;
  strcpy(wifi_status.ip, "192.168.1.50");
  strcpy(wifi_status.subnet, "255.255.255.0");
  strcpy(wifi_status.gateway, "192.168.1.1");
  strcpy(wifi_status.dns, "192.168.1.1");
  wifi_config.enabled = 1;
  wifi_config.password_present = 1;
  strcpy(wifi_config.ssid, "home");
  strcpy(wifi_password, "home-pass");
  wifi_status_err = wifi_adapter_err = wifi_set_err = wifi_scan_err = FN_OK;
  wifi_net_count = 0;
  wifi_set_calls = 0;
  wifi_scan_calls = 0;
  wifi_last_fields = 0;
}

fn_wifi_status_t *fake_wifi_status(void) { return &wifi_status; }
fn_wifi_config_t *fake_wifi_config(void) { return &wifi_config; }
void fake_wifi_status_error(uint8_t err) { wifi_status_err = err; }
void fake_wifi_adapter_error(uint8_t err) { wifi_adapter_err = err; }
void fake_wifi_set_error(uint8_t err) { wifi_set_err = err; }
void fake_wifi_scan_error(uint8_t err) { wifi_scan_err = err; }
unsigned fake_wifi_set_calls(void) { return wifi_set_calls; }
unsigned fake_wifi_scan_calls(void) { return wifi_scan_calls; }
uint8_t fake_wifi_last_fields(void) { return wifi_last_fields; }
const char *fake_wifi_password(void) { return wifi_password; }
const char *fake_wifi_bssid(void) { return wifi_config.bssid; }

void fake_wifi_add_network(const char *ssid, int8_t rssi, uint8_t auth)
{
  fn_wifi_scan_record_t *r;

  if (wifi_net_count >= FAKE_WIFI_NETS)
    return;
  r = &wifi_nets[wifi_net_count];
  memset(r, 0, sizeof(*r));
  strncpy(r->ssid, ssid, FN_WIFI_MAX_SSID);
  r->bssid.valid = 1;
  r->bssid.bytes[5] = wifi_net_count;
  r->rssi = rssi;
  r->channel = 6;
  r->auth = auth;
  wifi_net_count++;
}

uint8_t fn_wifi_get_status(fn_wifi_status_t *status)
{
  if (wifi_status_err)
    return wifi_status_err;
  *status = wifi_status;
  return FN_OK;
}

uint8_t fn_wifi_get_config(fn_wifi_config_t *config)
{
  if (wifi_status_err)
    return wifi_status_err;
  *config = wifi_config;
  return FN_OK;
}

uint8_t fn_wifi_get_adapter_info(fn_wifi_adapter_info_t *info)
{
  static const uint8_t mac[6] = { 0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF };

  if (wifi_adapter_err)
    return wifi_adapter_err;
  memset(info, 0, sizeof(*info));
  memcpy(info->mac.bytes, mac, 6);
  info->mac.valid = 1;
  strcpy(info->firmware, "0.1.1");
  return FN_OK;
}

uint8_t fn_wifi_set_config(const fn_wifi_config_update_t *u)
{
  wifi_set_calls++;
  wifi_last_fields = u->fields;
  if (wifi_set_err)
    return wifi_set_err;
  if ((u->fields & FN_WIFI_SET_SSID) && (!u->ssid || strlen(u->ssid) > FN_WIFI_MAX_SSID))
    return FN_ERR_INVALID;
  if ((u->fields & FN_WIFI_SET_PASSWORD) &&
      (!u->password || strlen(u->password) > FN_WIFI_MAX_PASSWORD))
    return FN_ERR_INVALID;
  if (u->fields & FN_WIFI_SET_ENABLED)
    wifi_config.enabled = u->enabled;
  if (u->fields & FN_WIFI_SET_SSID)
    strcpy(wifi_config.ssid, u->ssid);
  if (u->fields & FN_WIFI_SET_BSSID)
    strcpy(wifi_config.bssid, u->bssid);
  if (u->fields & FN_WIFI_SET_PASSWORD) {
    strcpy(wifi_password, u->password);
    wifi_config.password_present = u->password[0] != 0;
  }
  if (u->fields & FN_WIFI_SET_RECONNECT)
    wifi_status.link_state = 1;   /* connecting */
  return FN_OK;
}

uint8_t fn_wifi_scan(uint16_t offset, uint8_t limit, fn_wifi_scan_record_t *records,
                     uint8_t capacity, uint8_t *count, uint8_t *more,
                     uint8_t *response_buffer, uint16_t response_capacity)
{
  uint8_t n = limit < capacity ? limit : capacity;
  uint8_t i;

  (void) response_buffer;
  wifi_scan_calls++;
  if (!response_capacity || !n)
    return FN_ERR_INVALID;
  if (wifi_scan_err)
    return wifi_scan_err;
  if (n > FAKE_WIFI_PAGE)
    n = FAKE_WIFI_PAGE;
  if (offset > wifi_net_count)
    offset = wifi_net_count;
  if (n > wifi_net_count - offset)
    n = (uint8_t) (wifi_net_count - offset);
  for (i = 0; i < n; i++)
    records[i] = wifi_nets[offset + i];
  *count = n;
  *more = (uint8_t) (offset + n < wifi_net_count);
  return FN_OK;
}
