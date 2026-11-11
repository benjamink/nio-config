#include "config_nio.h"
#include "fujinet-nio.h"

#include <stdlib.h>
#include <string.h>

#if defined(__CC65__)
#define CONFIG_NIO_APPSTORE_BUF_SIZE 512
#else
#define CONFIG_NIO_APPSTORE_BUF_SIZE 1024
#endif

#define CONFIG_NIO_STORE_BUF_SIZE (CONFIG_NIO_TEXT_MAX + 1)
#define CONFIG_NIO_APPSTORE_READ_MAX (CONFIG_NIO_APPSTORE_BUF_SIZE - 10)
#define CONFIG_NIO_MAPPINGS_VERSION 1
#define CONFIG_NIO_MAPPING_VALID 0x01
#define CONFIG_NIO_MAPPING_READONLY 0x02
#define CONFIG_NIO_MAPPINGS_SIZE (1 + FNCTL_MAX_UNITS * 2)

static uint8_t store_buf[CONFIG_NIO_STORE_BUF_SIZE];
static uint8_t appstore_buf[CONFIG_NIO_APPSTORE_BUF_SIZE];
static fn_appstore_io_t appstore_io = { appstore_buf, sizeof(appstore_buf) };
static fn_slot_catalog_io_t slot_io = { appstore_buf, sizeof(appstore_buf) };
static char line_buf[CONFIG_NIO_URI_MAX + 10];
static char host_tmp[CONFIG_NIO_URI_MAX + 1];

static void append_digit(char *buf, uint16_t *off, uint8_t value)
{
  buf[*off] = (char) ('0' + value);
  *off = (uint16_t) (*off + 1);
  buf[*off] = 0;
}

static void append_uint(char *buf, uint16_t *off, uint8_t value)
{
  if (value >= 100)
    append_digit(buf, off, (uint8_t) (value / 100));
  if (value >= 10)
    append_digit(buf, off, (uint8_t) ((value / 10) % 10));
  append_digit(buf, off, (uint8_t) (value % 10));
}

static void append_text(char *buf, uint16_t *off, const char *text)
{
  while (text && *text) {
    buf[*off] = *text++;
    *off = (uint16_t) (*off + 1);
  }
  buf[*off] = 0;
}

/* The last app-store read error and which load step hit it, for the
 * failure message (see config_nio_load_error). */
static uint8_t load_error_code;
static const char *load_error_step = "";

const char *config_nio_load_error(uint8_t *code)
{
  if (code)
    *code = load_error_code;
  return load_error_step;
}

static int appstore_read_text(const char *key, char *buf, uint16_t cap,
                              uint8_t *exists)
{
  fn_appstore_read_t rr;
  uint16_t max_read;
  uint8_t result;
  uint16_t total;

  if (cap == 0)
    return 0;

  buf[0] = 0;
  if (exists)
    *exists = 0;

  max_read = (uint16_t) (cap - 1);
  if (max_read > CONFIG_NIO_APPSTORE_READ_MAX)
    max_read = CONFIG_NIO_APPSTORE_READ_MAX;

  total = 0;
  do {
    uint16_t want;
    want = (uint16_t) (cap - 1 - total);
    if (want > max_read)
      want = max_read;
    if (want == 0)
      return 0;
    result = fn_appstore_read(&appstore_io, CONFIG_NIO_NS, key, total,
                              appstore_buf, want, &rr);
    if (result != FN_OK) {
      load_error_code = result;
      return 0;
    }
    if ((rr.flags & FN_APPSTORE_READ_EXISTS) == 0)
      return 1;
    if (exists)
      *exists = 1;
    memcpy(buf + total, appstore_buf, rr.bytes_read);
    total = (uint16_t) (total + rr.bytes_read);
    buf[total] = 0;
  } while ((rr.flags & FN_APPSTORE_READ_EOF) == 0 && rr.bytes_read != 0);
  return 1;
}

static int appstore_write_text(const char *key, const char *buf)
{
  fn_appstore_write_t wr;
  uint16_t len;
  uint16_t off;

  len = (uint16_t) strlen(buf);
  off = 0;
  if (len == 0) {
    return fn_appstore_write(&appstore_io, CONFIG_NIO_NS, key, 0,
                             (const uint8_t *) "", 0, &wr) == FN_OK;
  }
  while (off < len) {
    uint16_t chunk;
    chunk = (uint16_t) (len - off);
    if (chunk > CONFIG_NIO_APPSTORE_READ_MAX)
      chunk = CONFIG_NIO_APPSTORE_READ_MAX;
    if (fn_appstore_write(&appstore_io, CONFIG_NIO_NS, key, off,
                          (const uint8_t *) buf + off, chunk, &wr) != FN_OK ||
        wr.bytes_written != chunk)
      return 0;
    off = (uint16_t) (off + chunk);
  }
  return 1;
}

static void trim_line(char *s)
{
  char *p;

  p = strchr(s, '\n');
  if (p)
    *p = 0;
  p = strchr(s, '\r');
  if (p)
    *p = 0;
}

static int next_line(const char **src, char *out, uint16_t cap)
{
  const char *p;
  uint16_t len;

  if (!src || !*src || !**src || cap == 0)
    return 0;

  p = *src;
  len = 0;
  while (p[len] && p[len] != '\n' && p[len] != '\r' && len < (uint16_t) (cap - 1)) {
    out[len] = p[len];
    len++;
  }
  out[len] = 0;

  while (p[len] && p[len] != '\n' && p[len] != '\r')
    len++;
  while (p[len] == '\n' || p[len] == '\r')
    len++;
  *src = &p[len];
  trim_line(out);
  return 1;
}

static void seed_hosts(config_nio_state_t *state)
{
  state->host_count = 3;
  (void) config_nio_host_set(state, 0, "sd0:/");
  (void) config_nio_host_set(state, 1, "fujinet.diller.org");
  (void) config_nio_host_set(state, 2, "fujinet.online");
}

static void seed_prefs(config_nio_state_t *state)
{
  config_nio_prefs_t *prefs;

  prefs = &state->prefs;
  prefs->date_format = CONFIG_NIO_PREF_DATE_YMD;
  prefs->size_format = CONFIG_NIO_PREF_SIZE_FULL;

#if !defined(__CC65__)
  prefs->color_fg[CONFIG_NIO_COLOR_BODY] = 7;
  prefs->color_bg[CONFIG_NIO_COLOR_BODY] = 1;
  prefs->color_fg[CONFIG_NIO_COLOR_FRAME] = 3;
  prefs->color_bg[CONFIG_NIO_COLOR_FRAME] = 1;
  prefs->color_fg[CONFIG_NIO_COLOR_TITLE] = 6;
  prefs->color_bg[CONFIG_NIO_COLOR_TITLE] = 1;
  prefs->color_fg[CONFIG_NIO_COLOR_SELECT] = 0;
  prefs->color_bg[CONFIG_NIO_COLOR_SELECT] = 3;
  prefs->color_fg[CONFIG_NIO_COLOR_STATUS] = 0;
  prefs->color_bg[CONFIG_NIO_COLOR_STATUS] = 3;
  prefs->color_fg[CONFIG_NIO_COLOR_INACTIVE] = 7;
  prefs->color_bg[CONFIG_NIO_COLOR_INACTIVE] = 1;
  prefs->color_fg[CONFIG_NIO_COLOR_INACTIVE_SELECT] = 7;
  prefs->color_bg[CONFIG_NIO_COLOR_INACTIVE_SELECT] = 5;
  prefs->color_fg[CONFIG_NIO_COLOR_MENUBAR] = 0;
  prefs->color_bg[CONFIG_NIO_COLOR_MENUBAR] = 7;
  prefs->color_fg[CONFIG_NIO_COLOR_MENUHOT] = 4;
  prefs->color_bg[CONFIG_NIO_COLOR_MENUHOT] = 7;
  prefs->color_fg[CONFIG_NIO_COLOR_TITLEBAR] = 0;
  prefs->color_bg[CONFIG_NIO_COLOR_TITLEBAR] = 3;
  prefs->color_fg[CONFIG_NIO_COLOR_BUTTON] = 0;
  prefs->color_bg[CONFIG_NIO_COLOR_BUTTON] = 7;
  prefs->color_fg[CONFIG_NIO_COLOR_BUTTON_SELECT] = 7;
  prefs->color_bg[CONFIG_NIO_COLOR_BUTTON_SELECT] = 1;
#else
  prefs->color_fg[0] = 7;
  prefs->color_bg[0] = 1;
#endif
}

static void parse_hosts(config_nio_state_t *state, const char *text)
{
  const char *p;

  state->host_count = 0;
  p = text;
  while (next_line(&p, line_buf, sizeof(line_buf)) &&
         state->host_count < CONFIG_NIO_MAX_HOSTS) {
    if (!line_buf[0])
      continue;
    strncpy(host_tmp, line_buf, CONFIG_NIO_URI_MAX);
    host_tmp[CONFIG_NIO_URI_MAX] = 0;
    (void) config_nio_host_set(state, state->host_count, host_tmp);
    state->host_count++;
  }
}

static void parse_mappings(config_nio_state_t *state, const uint8_t *data,
                           uint16_t len)
{
  uint8_t i;

  for (i = 0; i < FNCTL_MAX_UNITS; i++)
    (void) config_nio_mapping_clear(state, i);

  if (!data || len != CONFIG_NIO_MAPPINGS_SIZE ||
      data[0] != CONFIG_NIO_MAPPINGS_VERSION)
    return;

  for (i = 0; i < FNCTL_MAX_UNITS; i++) {
    uint8_t flags;
    config_nio_mapping_t mapping;

    flags = data[1 + i * 2];
    if ((flags & CONFIG_NIO_MAPPING_VALID) == 0)
      continue;
    memset(&mapping, 0, sizeof(mapping));
    mapping.valid = 1;
    mapping.slot = data[2 + i * 2];
    mapping.readonly =
      (uint8_t) ((flags & CONFIG_NIO_MAPPING_READONLY) != 0);
    (void) config_nio_mapping_set(state, i, &mapping);
  }
}

static int appstore_read_mappings(config_nio_state_t *state, uint8_t *exists)
{
  fn_appstore_read_t rr;
  uint8_t result;

  if (exists)
    *exists = 0;
  result = fn_appstore_read(&appstore_io, CONFIG_NIO_NS,
                            CONFIG_NIO_KEY_MAPPINGS, 0, appstore_buf,
                            CONFIG_NIO_MAPPINGS_SIZE, &rr);
  if (result != FN_OK) {
    load_error_code = result;
    return 0;
  }
  if ((rr.flags & FN_APPSTORE_READ_EXISTS) == 0)
    return 1;
  if (exists)
    *exists = 1;
  if ((rr.flags & FN_APPSTORE_READ_EOF) == 0)
    return 1;
  parse_mappings(state, appstore_buf, rr.bytes_read);
  return 1;
}

static void parse_prefs(config_nio_state_t *state, const char *text)
{
  const char *p;

  p = text;
  while (next_line(&p, line_buf, sizeof(line_buf))) {
    char *a;
    char *b;
    int index;
    int fg;
    int bg;

    if (strncmp(line_buf, "date=", 5) == 0) {
      if (strcmp(line_buf + 5, "ydm") == 0)
        state->prefs.date_format = CONFIG_NIO_PREF_DATE_YDM;
      else
        state->prefs.date_format = CONFIG_NIO_PREF_DATE_YMD;
      continue;
    }
    if (strncmp(line_buf, "size=", 5) == 0) {
      if (strcmp(line_buf + 5, "compact") == 0)
        state->prefs.size_format = CONFIG_NIO_PREF_SIZE_COMPACT;
      else
        state->prefs.size_format = CONFIG_NIO_PREF_SIZE_FULL;
      continue;
    }
    if (strncmp(line_buf, "color", 5) != 0)
      continue;
    a = strchr(line_buf, '=');
    if (!a)
      continue;
    *a++ = 0;
    b = strchr(a, ',');
    if (!b)
      continue;
    *b++ = 0;
    index = atoi(line_buf + 5);
    fg = atoi(a);
    bg = atoi(b);
    if (index < 0 || index >= CONFIG_NIO_COLOR_COUNT ||
        fg < 0 || fg > 7 || bg < 0 || bg > 7)
      continue;
    state->prefs.color_fg[index] = (uint8_t) fg;
    state->prefs.color_bg[index] = (uint8_t) bg;
  }
}

int config_nio_save_hosts(const config_nio_state_t *state)
{
  uint8_t i;
  uint16_t off;

  off = 0;
  store_buf[0] = 0;
  for (i = 0; i < state->host_count; i++) {
    uint16_t len;
    char host[CONFIG_NIO_URI_MAX + 1];

    if (!config_nio_host_get(state, i, host, sizeof(host)))
      continue;
    len = (uint16_t) strlen(host);
    if ((uint16_t) (off + len + 1) >= sizeof(store_buf))
      return 0;
    memcpy(&store_buf[off], host, len);
    off = (uint16_t) (off + len);
    store_buf[off++] = '\n';
    store_buf[off] = 0;
  }
  return appstore_write_text(CONFIG_NIO_KEY_HOSTS, (const char *) store_buf);
}

int config_nio_save_mappings(const config_nio_state_t *state)
{
  fn_appstore_delete_t dr;
  fn_appstore_write_t wr;
  uint8_t unit;

  store_buf[0] = CONFIG_NIO_MAPPINGS_VERSION;
  memset(&store_buf[1], 0, CONFIG_NIO_MAPPINGS_SIZE - 1);
  for (unit = 0; unit < FNCTL_MAX_UNITS; unit++) {
    config_nio_mapping_t mapping;
    uint8_t flags;

    if (!config_nio_mapping_get(state, unit, &mapping) || !mapping.valid)
      continue;
    flags = CONFIG_NIO_MAPPING_VALID;
    if (mapping.readonly)
      flags |= CONFIG_NIO_MAPPING_READONLY;
    store_buf[1 + unit * 2] = flags;
    store_buf[2 + unit * 2] = mapping.slot;
  }

  if (fn_appstore_delete(&appstore_io, CONFIG_NIO_NS,
                         CONFIG_NIO_KEY_MAPPINGS, &dr) != FN_OK)
    return 0;
  return fn_appstore_write(&appstore_io, CONFIG_NIO_NS,
                           CONFIG_NIO_KEY_MAPPINGS, 0, store_buf,
                           CONFIG_NIO_MAPPINGS_SIZE, &wr) == FN_OK &&
         wr.bytes_written == CONFIG_NIO_MAPPINGS_SIZE;
}

int config_nio_read_slot(uint8_t index, config_nio_slot_t *slot)
{
  fn_slot_catalog_entry_t entry;
  uint8_t result;
  uint16_t uri_len;
  if (!slot)
    return 0;
  memset(slot, 0, sizeof(*slot));
  result = fn_slot_catalog_get(&slot_io, index, &entry);
  if (result == FN_ERR_NOT_FOUND)
    return 1;
  if (result != FN_OK)
    return 0;
  if ((entry.flags & FN_SLOT_CATALOG_ENTRY_VALID) == 0)
    return 0;
  uri_len = entry.uri_len;
  if (uri_len > CONFIG_NIO_URI_MAX)
    uri_len = CONFIG_NIO_URI_MAX;
  slot->enabled = 1;
  strcpy(slot->mode,
         (entry.flags & FN_SLOT_CATALOG_ENTRY_READ_ONLY) ? "r" : "rw");
  memcpy(slot->uri, entry.uri, uri_len);
  slot->uri[uri_len] = 0;
  return slot->uri[0] != 0;
}

int config_nio_refresh_slots(config_nio_state_t *state)
{
  uint8_t i;
  if (!state)
    return 0;
  state->slot_count = 0;
  for (i = 0; i < FNCTL_MAX_UNITS; i++) {
    uint16_t absolute;
    absolute = (uint16_t) state->slot_start + i;
    if (absolute > 255) {
      memset(&state->slots[i], 0, sizeof(state->slots[i]));
      continue;
    }
    if (!config_nio_read_slot((uint8_t) absolute, &state->slots[i]))
      return 0;
    if (state->slots[i].enabled)
      state->slot_count++;
  }
  state->slots_more = state->slot_start < 248;
  return 1;
}

int config_nio_write_slot(config_nio_state_t *state, uint8_t index,
                          const char *uri, const char *mode)
{
  fn_slot_catalog_entry_t entry;
  uint16_t uri_len;
  uint8_t flags;
  if (!state || !uri || !uri[0])
    return 0;
  uri_len = (uint16_t) strlen(uri);
  if (uri_len > CONFIG_NIO_URI_MAX ||
      (size_t) uri_len + 5 > sizeof(appstore_buf))
    return 0;
  flags = (uint8_t) (mode && strcmp(mode, "r") == 0
                     ? FN_SLOT_CATALOG_ENTRY_READ_ONLY : 0);
  if (fn_slot_catalog_put(&slot_io, index, flags, uri, &entry) != FN_OK)
    return 0;
  return config_nio_refresh_slots(state);
}

int config_nio_delete_slot(config_nio_state_t *state, uint8_t index)
{
  uint8_t deleted;
  if (!state)
    return 0;
  if (fn_slot_catalog_delete(&slot_io, index, &deleted) != FN_OK)
    return 0;
  return config_nio_refresh_slots(state);
}

int config_nio_save_prefs(const config_nio_state_t *state)
{
  uint8_t i;
  uint16_t off;
  const char *date_value;
  const char *size_value;

  off = 0;
  store_buf[0] = 0;
  date_value = state->prefs.date_format == CONFIG_NIO_PREF_DATE_YDM ? "ydm" : "ymd";
  size_value = state->prefs.size_format == CONFIG_NIO_PREF_SIZE_COMPACT ? "compact" : "full";
  append_text((char *) store_buf, &off, "date=");
  append_text((char *) store_buf, &off, date_value);
  append_text((char *) store_buf, &off, "\nsize=");
  append_text((char *) store_buf, &off, size_value);
  append_text((char *) store_buf, &off, "\n");
  for (i = 0; i < CONFIG_NIO_COLOR_COUNT; i++) {
    if ((uint16_t) (off + 16) >= sizeof(store_buf))
      return 0;
    append_text((char *) store_buf, &off, "color");
    append_uint((char *) store_buf, &off, i);
    store_buf[off++] = '=';
    store_buf[off] = 0;
    append_uint((char *) store_buf, &off, state->prefs.color_fg[i]);
    store_buf[off++] = ',';
    store_buf[off] = 0;
    append_uint((char *) store_buf, &off, state->prefs.color_bg[i]);
    store_buf[off++] = '\n';
    store_buf[off] = 0;
  }
  return appstore_write_text(CONFIG_NIO_KEY_PREFS, (const char *) store_buf);
}

int config_nio_load(config_nio_state_t *state)
{
  uint8_t exists;

  if (!state)
    return 0;

  memset(state, 0, sizeof(*state));
  seed_prefs(state);
  load_error_code = 0;
  load_error_step = "host list";
  if (!appstore_read_text(CONFIG_NIO_KEY_HOSTS, (char *) store_buf,
                          sizeof(store_buf), &exists))
    return 0;
  if (exists)
    parse_hosts(state, (const char *) store_buf);
  if (state->host_count == 0) {
    seed_hosts(state);
    (void) config_nio_save_hosts(state);
  }

  load_error_step = "drive mappings";
  if (!appstore_read_mappings(state, &exists))
    return 0;

  load_error_step = "preferences";
  if (!appstore_read_text(CONFIG_NIO_KEY_PREFS, (char *) store_buf,
                          sizeof(store_buf), &exists))
    return 0;
  if (exists)
    parse_prefs(state, (const char *) store_buf);

  (void) config_nio_refresh_slots(state);
  config_nio_set_status(state, "Ready");
  return 1;
}
