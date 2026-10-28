#include "atari_core.h"

#include "fn_raw.h"
#include "fujinet-nio.h"

#include <string.h>

#define KEY_HOSTS    "hosts"
#define KEY_MAPPINGS "mappings"
#define MAPPINGS_VERSION 1
#define MAPPING_VALID    0x01
#define MAPPING_READONLY 0x02
#define MAPPINGS_SIZE (1 + CORE_UNITS * 2)
#define TEXT_MAX ((CORE_URI_MAX + 1) * CORE_MAX_HOSTS)

#define FILE_DEVICE 0xFE
#define FILE_LIST   0x02
#define LIST_SORT   0x02
#define LIST_PAYLOAD 420

char hosts[CORE_MAX_HOSTS][CORE_URI_MAX + 1];
uint8_t host_count;
uint8_t drive_on[CORE_UNITS];
uint8_t drive_ro[CORE_UNITS];
uint8_t drive_slot[CORE_UNITS];
char drive_uri[CORE_UNITS][CORE_URI_MAX + 1];
core_entry_t entries[CORE_WINDOW];
uint8_t entry_count;
uint8_t entry_more;
uint16_t entry_start;
uint8_t list_stale;

/* One buffer serves the app store, the slot catalogue and directory pages. */
static uint8_t iobuf[512];
static fn_appstore_io_t app_io = { iobuf, sizeof(iobuf) };
static fn_slot_catalog_io_t slot_io = { iobuf, sizeof(iobuf) };
/* Host text is built in the directory page buffer, which is idle then. */
#define text ((char *) entries)
uint8_t core_error;
static uint8_t list_error;
static uint8_t mount_error;
static uint8_t mount_result;
static char mount_text[32];

/* ---- hosts ------------------------------------------------------------------ */

static uint8_t read_text(const char *key)
{
  fn_appstore_read_t rr;
  uint16_t total;

  total = 0;
  text[0] = 0;
  for (;;) {
    uint16_t want;

    want = (uint16_t) (TEXT_MAX - total);
    if (want > sizeof(iobuf) - 16)
      want = (uint16_t) (sizeof(iobuf) - 16);
    if (!want)
      break;
    core_error = fn_appstore_read(&app_io, CORE_NS, key, total, iobuf, want, &rr);
    if (core_error != FN_OK)
      return 0;
    if (!(rr.flags & FN_APPSTORE_READ_EXISTS) || !rr.bytes_read)
      break;
    memcpy(text + total, iobuf, rr.bytes_read);
    total = (uint16_t) (total + rr.bytes_read);
    text[total] = 0;
    if (rr.flags & FN_APPSTORE_READ_EOF)
      break;
  }
  return 1;
}

static void parse_hosts(void)
{
  const char *p;

  host_count = 0;
  p = text;
  while (*p && host_count < CORE_MAX_HOSTS) {
    char *d;
    uint8_t n;

    d = hosts[host_count];
    n = 0;
    while (*p && *p != '\n' && *p != '\r') {
      if (n < CORE_URI_MAX)
        d[n++] = *p;
      p++;
    }
    d[n] = 0;
    while (*p == '\n' || *p == '\r')
      p++;
    if (n)
      host_count++;
  }
}

void *core_scratch(void)
{
  list_stale = 1;
  return entries;
}

uint8_t core_save_hosts(void)
{
  fn_appstore_write_t wr;
  uint16_t off;
  uint16_t chunk;
  uint8_t i;

  list_stale = 1;
  off = 0;
  for (i = 0; i < host_count; i++) {
    uint8_t n;

    n = (uint8_t) strlen(hosts[i]);
    memcpy(text + off, hosts[i], n);
    off = (uint16_t) (off + n);
    text[off++] = '\n';
  }
  if (!off)
    return fn_appstore_write(&app_io, CORE_NS, KEY_HOSTS, 0, iobuf, 0, &wr) == FN_OK;
  for (chunk = 0; chunk < off;) {
    uint16_t n;

    n = (uint16_t) (off - chunk);
    if (n > 400)
      n = 400;
    if (fn_appstore_write(&app_io, CORE_NS, KEY_HOSTS, chunk,
                          (const uint8_t *) text + chunk, n, &wr) != FN_OK ||
        wr.bytes_written != n)
      return 0;
    chunk = (uint16_t) (chunk + n);
  }
  return 1;
}

/* ---- drive map ---------------------------------------------------------------- */

static void load_slot(uint8_t unit)
{
  fn_slot_catalog_entry_t e;
  uint16_t n;

  drive_uri[unit][0] = 0;
  if (fn_slot_catalog_get(&slot_io, drive_slot[unit], &e) != FN_OK ||
      !(e.flags & FN_SLOT_CATALOG_ENTRY_VALID)) {
    drive_on[unit] = 0;
    return;
  }
  n = e.uri_len;
  if (n > CORE_URI_MAX)
    n = CORE_URI_MAX;
  memcpy(drive_uri[unit], e.uri, n);
  drive_uri[unit][n] = 0;
  if (e.flags & FN_SLOT_CATALOG_ENTRY_READ_ONLY)
    drive_ro[unit] = 1;
}

static void load_drives(void)
{
  fn_appstore_read_t rr;
  uint8_t u;

  memset(drive_on, 0, sizeof(drive_on));
  memset(drive_ro, 0, sizeof(drive_ro));
  for (u = 0; u < CORE_UNITS; u++)
    drive_slot[u] = u;
  if (fn_appstore_read(&app_io, CORE_NS, KEY_MAPPINGS, 0, iobuf, MAPPINGS_SIZE,
                       &rr) != FN_OK ||
      !(rr.flags & FN_APPSTORE_READ_EXISTS) || rr.bytes_read != MAPPINGS_SIZE ||
      iobuf[0] != MAPPINGS_VERSION)
    return;
  for (u = 0; u < CORE_UNITS; u++) {
    uint8_t flags;

    flags = iobuf[1 + u * 2];
    drive_slot[u] = iobuf[2 + u * 2];
    drive_on[u] = (uint8_t) ((flags & MAPPING_VALID) != 0);
    drive_ro[u] = (uint8_t) ((flags & MAPPING_READONLY) != 0);
  }
  for (u = 0; u < CORE_UNITS; u++)
    if (drive_on[u])
      load_slot(u);
}

uint8_t core_save_drives(void)
{
  fn_appstore_delete_t dr;
  fn_appstore_write_t wr;
  uint8_t map[MAPPINGS_SIZE];
  uint8_t u;

  map[0] = MAPPINGS_VERSION;
  for (u = 0; u < CORE_UNITS; u++) {
    map[1 + u * 2] = (uint8_t) (drive_on[u]
                                ? MAPPING_VALID | (drive_ro[u] ? MAPPING_READONLY : 0)
                                : 0);
    map[2 + u * 2] = drive_on[u] ? drive_slot[u] : 0;
  }
  if (fn_appstore_delete(&app_io, CORE_NS, KEY_MAPPINGS, &dr) != FN_OK)
    return 0;
  return fn_appstore_write(&app_io, CORE_NS, KEY_MAPPINGS, 0, map,
                           MAPPINGS_SIZE, &wr) == FN_OK &&
         wr.bytes_written == MAPPINGS_SIZE;
}

uint8_t core_put_slot(uint8_t slot, const char *uri, uint8_t readonly)
{
  fn_slot_catalog_entry_t e;

  return fn_slot_catalog_put(&slot_io, slot,
                             readonly ? FN_SLOT_CATALOG_ENTRY_READ_ONLY : 0,
                             uri, &e) == FN_OK;
}

uint8_t core_load(void)
{
  if (!read_text(KEY_HOSTS))
    return 0;
  parse_hosts();
  if (!host_count) {
    strcpy(hosts[0], "sd0:/");
    strcpy(hosts[1], "fujinet.diller.org");
    strcpy(hosts[2], "fujinet.online");
    host_count = 3;
    (void) core_save_hosts();
  }
  load_drives();
  return 1;
}

/* ---- URIs ------------------------------------------------------------------------ */

static uint8_t has_scheme(const char *s)
{
  while (*s && *s != '/' && *s != '\\') {
    if (*s == ':')
      return 1;
    s++;
  }
  return 0;
}

static uint8_t append(char *out, uint8_t cap, uint8_t *pos, const char *s)
{
  while (*s) {
    if ((uint8_t) (*pos + 1) >= cap)
      return 0;
    out[(*pos)++] = *s++;
  }
  out[*pos] = 0;
  return 1;
}

uint8_t core_compose(char *out, uint8_t cap, const char *host,
                     const char *path, const char *leaf)
{
  uint8_t pos;

  pos = 0;
  out[0] = 0;
  if (!*host)
    return 0;
  if (!has_scheme(host) && !append(out, cap, &pos, "tnfs://"))
    return 0;
  if (!append(out, cap, &pos, host))
    return 0;
  if (pos && out[pos - 1] != '/' && !append(out, cap, &pos, "/"))
    return 0;
  if (!append(out, cap, &pos, path))
    return 0;
  if (*leaf) {
    if (pos && out[pos - 1] != '/' && !append(out, cap, &pos, "/"))
      return 0;
    if (!append(out, cap, &pos, leaf))
      return 0;
  }
  return 1;
}

/* ---- directory listing -------------------------------------------------------------- */

uint8_t core_list_error(void)
{
  return list_error;
}

/* One ListDirectory request from entry `start`, appended to entries[].  The
 * device returns as many whole entries as fit in LIST_PAYLOAD bytes, which is
 * fewer than a window with long names.  Sets entry_more if the directory has
 * more after what was taken. */
static uint8_t list_once(const char *uri, uint16_t start, uint8_t *taken)
{
  uint8_t req[6 + CORE_URI_MAX + 12 + 4 + 8];
  fn_raw_response_t raw;
  uint16_t ulen;
  uint16_t off;
  uint16_t count;
  uint16_t pos;
  uint16_t i;

  *taken = 0;
  ulen = (uint16_t) strlen(uri);
  if (ulen > sizeof(req) - 12) {
    list_error = 1;
    return 0;
  }
  off = 0;
  req[off++] = 1;
  req[off++] = (uint8_t) ulen;
  req[off++] = (uint8_t) (ulen >> 8);
  memcpy(req + off, uri, ulen);
  off = (uint16_t) (off + ulen);
  req[off++] = (uint8_t) start;
  req[off++] = (uint8_t) (start >> 8);
  req[off++] = (uint8_t) LIST_PAYLOAD;
  req[off++] = (uint8_t) (LIST_PAYLOAD >> 8);
  req[off++] = LIST_SORT;
  req[off++] = CORE_NAME_MAX;

  if (fn_raw_call(FILE_DEVICE, FILE_LIST, req, off, iobuf, sizeof(iobuf),
                  &raw) != FN_OK) {
    list_error = 2;
    return 0;
  }
  if (raw.status != 0 || raw.payload_length < 10 || iobuf[0] != 1) {
    list_error = 3;
    return 0;
  }
  entry_more = (uint8_t) (iobuf[1] & 1);
  count = (uint16_t) (iobuf[6] | (iobuf[7] << 8));
  pos = 10;
  for (i = 0; i < count; i++) {
    core_entry_t *e;
    uint8_t n;

    if (entry_count >= CORE_WINDOW) {
      entry_more = 1;           /* a page entry did not fit the window */
      break;
    }
    if ((uint16_t) (pos + 2) > raw.payload_length)
      break;
    e = &entries[entry_count];
    e->flags = iobuf[pos++];
    n = iobuf[pos++];
    if ((uint16_t) (pos + n) > raw.payload_length)
      break;
    if (n > CORE_NAME_MAX) {
      memcpy(e->name, iobuf + pos, CORE_NAME_MAX);
      e->name[CORE_NAME_MAX] = 0;
      e->flags |= CORE_ENTRY_TRUNCATED;
    } else {
      memcpy(e->name, iobuf + pos, n);
      e->name[n] = 0;
    }
    pos = (uint16_t) (pos + n);
    if (iobuf[1] & 2) {
      e->size = 0;
    } else {
      if ((uint16_t) (pos + 16) > raw.payload_length)
        break;
      memcpy(&e->size, iobuf + pos, 4);
      pos = (uint16_t) (pos + 16);
    }
    entry_count++;
    (*taken)++;
  }
  if (*taken < count)
    entry_more = 1;
  return 1;
}

uint8_t core_list(const char *uri, uint16_t start)
{
  uint8_t taken;

  entry_count = 0;
  entry_more = 0;
  entry_start = start;
  list_error = 0;
  list_stale = 0;
  /* Keep asking until the window is full or the directory ends. */
  do {
    if (!list_once(uri, (uint16_t) (start + entry_count), &taken))
      return 0;
  } while (entry_more && entry_count < CORE_WINDOW && taken);
  return 1;
}

/* ---- mounting ------------------------------------------------------------------------- */

const char *core_mount_error(void)
{
  switch (mount_error) {
  case FN_DISK_ERR_GEOMETRY_REQUIRED: return "Mount: sector size needed";
  case FN_DISK_ERR_INVALID_GEOMETRY:  return "Mount: wrong sector size";
  case FN_DISK_ERR_FILE_NOT_FOUND:    return "Mount: image not found";
  case FN_DISK_ERR_BAD_IMAGE:         return "Mount: bad image";
  case FN_DISK_ERR_UNSUPPORTED_TYPE:  return "Mount: unsupported type";
  default:
    strcpy(mount_text, "Mount failed, error ");
    mount_text[20] = (char) ('0' + mount_result / 100);
    mount_text[21] = (char) ('0' + mount_result / 10 % 10);
    mount_text[22] = (char) ('0' + mount_result % 10);
    mount_text[23] = 0;
    return mount_text;
  }
}

uint8_t core_mount_all(uint8_t *failed)
{
  fn_disk_info_t info;
  uint8_t u;
  uint8_t mounted;

  mounted = 0;
  *failed = 0xFF;
  for (u = 0; u < CORE_UNITS; u++) {
    if (!drive_on[u]) {
      if (u)                    /* D1 keeps the config disk until replaced */
        (void) fn_disk_unmount((uint8_t) (u + 1));
      continue;
    }
    mount_result = fn_disk_mount((uint8_t) (u + 1), drive_uri[u], drive_ro[u], FN_DISK_TYPE_AUTO,
                                 256, &info);
    if (mount_result != FN_OK) {
      mount_error = fn_disk_last_error();
      if (*failed == 0xFF)
        *failed = u;
      continue;
    }
    mounted++;
  }
  return mounted;
}
