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

#define FILE_DEVICE 0xFE
#define FILE_LIST   0x02
#define LIST_SORT   0x02
#define LIST_PAYLOAD 420

uint8_t drive_on[CORE_UNITS];
uint8_t drive_ro[CORE_UNITS];
uint8_t drive_slot[CORE_UNITS];
char drive_uri[CORE_UNITS][CORE_DRIVE_URI + 1];

void core_drive_tail(uint8_t unit, const char *uri, uint16_t len)
{
  if (len > CORE_DRIVE_URI) {
    uri += len - CORE_DRIVE_URI;
    len = CORE_DRIVE_URI;
  }
  memcpy(drive_uri[unit], uri, len);
  drive_uri[unit][len] = 0;
}

/* One buffer serves the app store, the slot catalogue and directory pages. */
static uint8_t iobuf[512];
static fn_appstore_io_t app_io = { iobuf, sizeof(iobuf) };
static fn_slot_catalog_io_t slot_io = { iobuf, sizeof(iobuf) };
uint8_t core_error;
static uint8_t list_error;
static uint8_t mount_error;
static uint8_t mount_result;
#pragma bss-name (push, "LOWMEM")
static char mount_text[32];
#pragma bss-name (pop)

/* ---- hosts ------------------------------------------------------------------ */

/* The saved hosts are one app-store record: text, a host per line.  In RAM they
 * are packed, NUL-separated, in one pool, so the count is limited by their
 * total length rather than by a fixed slot each: many short hosts or a few long
 * ones.  host_ptr[] indexes the pool (host_ptr[host_count] is its end). */
static char host_pool[CORE_HOST_POOL];
static uint16_t pool_len;
#pragma bss-name (push, "LOWMEM")
char *host_ptr[CORE_MAX_HOSTS + 1];
#pragma bss-name (pop)
uint8_t host_count;

static void hosts_index(void)
{
  char *p;
  uint8_t i;

  p = host_pool;
  for (i = 0; i < host_count; i++) {
    host_ptr[i] = p;
    p += strlen(p) + 1;
  }
  host_ptr[host_count] = p;
}

uint8_t core_host_set(uint8_t i, const char *s)
{
  uint16_t len;
  uint16_t old;
  char *at;

  if (i > host_count || (i == host_count && host_count >= CORE_MAX_HOSTS))
    return 0;
  len = (uint16_t) (strlen(s) + 1);
  at = host_ptr[i];
  old = i < host_count ? (uint16_t) (strlen(at) + 1) : 0;
  if ((uint16_t) (pool_len - old + len) > CORE_HOST_POOL)
    return 0;
  memmove(at + len, at + old, (uint16_t) (pool_len - (at + old - host_pool)));
  memcpy(at, s, len);
  pool_len = (uint16_t) (pool_len - old + len);
  if (i == host_count)
    host_count++;
  hosts_index();
  return 1;
}

void core_host_delete(uint8_t i)
{
  uint16_t old;
  char *at;

  at = host_ptr[i];
  old = (uint16_t) (strlen(at) + 1);
  memmove(at, at + old, (uint16_t) (pool_len - (at + old - host_pool)));
  pool_len = (uint16_t) (pool_len - old);
  host_count--;
  hosts_index();
}

/* Take the saved hosts text a chunk at a time (it can be longer than any
 * buffer here), adding each line to the pool. */
static uint8_t feed_len;

static void host_feed(const uint8_t *p, uint16_t n)
{
  while (n--) {
    uint8_t c;

    c = *p++;
    if (c == '\n' || c == '\r') {
      if (feed_len) {
        host_pool[pool_len++] = 0;
        host_count++;
        feed_len = 0;
      }
    } else if (feed_len < CORE_HOST_MAX && host_count < CORE_MAX_HOSTS &&
               pool_len + 2 < CORE_HOST_POOL) {
      host_pool[pool_len++] = (char) c;
      feed_len++;
    }
  }
}

static uint8_t read_hosts(void)
{
  fn_appstore_read_t rr;
  uint16_t total;

  total = 0;
  pool_len = 0;
  host_count = 0;
  feed_len = 0;
  for (;;) {
    core_error = fn_appstore_read(&app_io, CORE_NS, KEY_HOSTS, total, iobuf,
                                  (uint16_t) (sizeof(iobuf) - 16), &rr);
    if (core_error != FN_OK)
      return 0;
    if (!(rr.flags & FN_APPSTORE_READ_EXISTS) || !rr.bytes_read)
      break;
    host_feed(iobuf, rr.bytes_read);
    total = (uint16_t) (total + rr.bytes_read);
    if (rr.flags & FN_APPSTORE_READ_EOF)
      break;
  }
  if (feed_len) {                 /* last line had no newline */
    host_pool[pool_len++] = 0;
    host_count++;
    feed_len = 0;
  }
  hosts_index();
  return 1;
}

void *core_scratch(void)
{
  list_stale = 1;
  return entries;
}

uint8_t core_save_hosts(void)
{
  fn_appstore_write_t wr;
  char *t;
  uint16_t fill;
  uint16_t sent;
  uint8_t i;

  list_stale = 1;
  t = (char *) entries;           /* the page buffer: a 400 byte chunk at a time */
  fill = 0;
  sent = 0;
  if (!host_count)
    return fn_appstore_write(&app_io, CORE_NS, KEY_HOSTS, 0, iobuf, 0, &wr) == FN_OK;
  for (i = 0; i < host_count; i++) {
    uint16_t n;

    n = (uint16_t) (strlen(host_ptr[i]) + 1);
    if ((uint16_t) (fill + n) > 400) {
      if (fn_appstore_write(&app_io, CORE_NS, KEY_HOSTS, sent,
                            (const uint8_t *) t, fill, &wr) != FN_OK ||
          wr.bytes_written != fill)
        return 0;
      sent = (uint16_t) (sent + fill);
      fill = 0;
    }
    memcpy(t + fill, host_ptr[i], n - 1);
    t[fill + n - 1] = '\n';
    fill = (uint16_t) (fill + n);
  }
  return fn_appstore_write(&app_io, CORE_NS, KEY_HOSTS, sent,
                           (const uint8_t *) t, fill, &wr) == FN_OK &&
         wr.bytes_written == fill;
}

/* ---- drive map ---------------------------------------------------------------- */

static void load_slot(uint8_t unit)
{
  fn_slot_catalog_entry_t e;

  drive_uri[unit][0] = 0;
  if (fn_slot_catalog_get(&slot_io, drive_slot[unit], &e) != FN_OK ||
      !(e.flags & FN_SLOT_CATALOG_ENTRY_VALID)) {
    drive_on[unit] = 0;
    return;
  }
  core_drive_tail(unit, (const char *) e.uri, e.uri_len);
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
  if (!read_hosts())
    return 0;
  if (!host_count) {
    (void) core_host_set(0, "sd0:/");
    (void) core_host_set(1, "fujinet.diller.org");
    (void) core_host_set(2, "fujinet.online");
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

/* ---- directory pages ----------------------------------------------------------------- */

/* The device lists, sorts and remembers a directory and hands out whole pages
 * of it (fn_file_list_pages).  A screen page (CORE_PAGE_ROWS entries) is made
 * of two protocol pages of CORE_PROTO_PAGE entries, so that one reply, which is
 * limited to what fits the 512 byte transport, can carry whole pages whatever
 * the names look like.  Replies are kept as received in a small cache, so
 * going back to a page is instant; only reaching a page not seen yet touches
 * the network. */
#define LISTING_FLAGS (FN_LIST_SORT | FN_LIST_DIRS_FIRST)
/* The whole encoded reply (the 12 byte page header, the pages and the FujiBus
 * framing) must fit the 512 byte transport, or the device answers with an error. */
#define LIST_BUDGET   (sizeof(iobuf) - 12 - 32)
#define PAGE_HEADER   5         /* cache record: u16 page, u8 count, u16 len */

uint32_t dir_total;
core_entry_t entries[CORE_PAGE_ROWS];
uint8_t entry_count;
uint8_t list_stale;

#pragma bss-name (push, "LOWMEM")
static uint8_t pc[CORE_PCACHE];       /* in low memory: see makefiles/cc65-atari-nio.cfg */
#pragma bss-name (pop)
static uint16_t pc_used;
static fn_file_io_t file_io = { iobuf, sizeof(iobuf) };
static fn_list_pages_req_t rq = { 0, CORE_PROTO_PAGE, LIST_BUDGET, LISTING_FLAGS, CORE_NAME_MAX, 0 };

uint8_t core_list_error(void)
{
  return list_error;
}

void core_dir_reset(void)
{
  pc_used = 0;
  dir_total = 0xFFFFFFFFUL;
  entry_count = 0;
}

static uint8_t *pc_find(uint16_t page)
{
  uint16_t at;

  for (at = 0; at < pc_used;) {
    if ((uint16_t) (pc[at] | (pc[at + 1] << 8)) == page)
      return pc + at;
    at = (uint16_t) (at + PAGE_HEADER + (pc[at + 3] | (pc[at + 4] << 8)));
  }
  return 0;
}

/* Keep a received page, dropping the oldest ones if there is no room. */
static void pc_put(uint16_t page, uint8_t count, uint16_t len, const uint8_t *data)
{
  uint16_t need;
  uint16_t first;

  need = (uint16_t) (PAGE_HEADER + len);
  if (need > CORE_PCACHE || pc_find(page))
    return;
  while ((uint16_t) (pc_used + need) > CORE_PCACHE) {
    first = (uint16_t) (PAGE_HEADER + (pc[3] | (pc[4] << 8)));
    memmove(pc, pc + first, (uint16_t) (pc_used - first));
    pc_used = (uint16_t) (pc_used - first);
  }
  pc[pc_used] = (uint8_t) page;
  pc[pc_used + 1] = (uint8_t) (page >> 8);
  pc[pc_used + 2] = count;
  pc[pc_used + 3] = (uint8_t) len;
  pc[pc_used + 4] = (uint8_t) (len >> 8);
  memcpy(pc + pc_used + PAGE_HEADER, data, len);
  pc_used = (uint16_t) (pc_used + need);
}

/* Ask the device for protocol pages from `page` and keep what comes back. */
static uint8_t fetch(const char *uri, const char *filter, uint16_t page, uint8_t flags)
{
  fn_list_pages_t r;
  const uint8_t *p;
  uint8_t k;
  uint8_t res;

  rq.start_page = page;
  rq.page_size = CORE_PROTO_PAGE;
  rq.flags = (uint8_t) (LISTING_FLAGS | flags);
  rq.max_name = CORE_NAME_MAX;
  rq.filter = filter;
  res = fn_file_list_pages(&file_io, uri, &rq, &r);
  if (res != FN_OK) {
    list_error = res;
    return 0;
  }
  dir_total = r.total;
  p = r.pages;
  for (k = 0; k < r.page_count; k++) {
    uint16_t len;

    len = (uint16_t) (p[1] | (p[2] << 8));
    pc_put((uint16_t) (r.start_page + k), p[0], len, p + 3);
    p = p + 3 + len;
  }
  return 1;
}

/* Unpack one cached protocol page onto entries[]. */
static void unpack(const uint8_t *rec)
{
  const uint8_t *d;
  uint8_t count;

  count = rec[2];
  d = rec + PAGE_HEADER;
  while (count-- && entry_count < CORE_PAGE_ROWS) {
    core_entry_t *e;
    uint8_t n;
    uint32_t kb;

    e = &entries[entry_count++];
    e->flags = (uint8_t) (d[0] & (CORE_ENTRY_DIR | CORE_ENTRY_TRUNCATED));
    n = d[1];
    if (n > CORE_NAME_MAX)
      n = CORE_NAME_MAX;
    memcpy(e->name, d + 2, n);
    e->name[n] = 0;
    d = d + 2 + d[1];
    memcpy(&e->size, d, 4);
    memcpy(&e->mtime, d + 4, 4);
    d += 8;
    kb = (e->size + 1023UL) >> 10;     /* what the list shows, worked out once */
    if (kb >= 10000UL) {
      e->disp = (uint16_t) ((e->size + 0xFFFFFUL) >> 20);
      e->flags |= CORE_ENTRY_MB;
    } else {
      e->disp = (uint16_t) kb;
    }
  }
}

uint8_t core_dir_have(uint16_t screen_page)
{
  uint16_t first;
  uint8_t k;

  first = (uint16_t) (screen_page * (CORE_PAGE_ROWS / CORE_PROTO_PAGE));
  for (k = 0; k < CORE_PAGE_ROWS / CORE_PROTO_PAGE; k++) {
    if (dir_total != 0xFFFFFFFFUL && (uint32_t) (first + k) * CORE_PROTO_PAGE >= dir_total)
      break;
    if (!pc_find((uint16_t) (first + k)))
      return 0;
  }
  return 1;
}

uint8_t core_dir_page(const char *uri, const char *filter, uint16_t screen_page, uint8_t refresh)
{
  uint16_t first;
  uint8_t k;
  uint8_t *rec;

  list_error = 0;
  first = (uint16_t) (screen_page * (CORE_PAGE_ROWS / CORE_PROTO_PAGE));
  /* Unpack each page before fetching the next: the smaller cache need not
   * hold both maximum-length pages at once. */
  entry_count = 0;
  for (k = 0; k < CORE_PAGE_ROWS / CORE_PROTO_PAGE; k++) {
    uint16_t page;

    page = (uint16_t) (first + k);
    if (dir_total != 0xFFFFFFFFUL && (uint32_t) page * CORE_PROTO_PAGE >= dir_total)
      break;                    /* past the end of the directory */
    if (!pc_find(page)) {
      if (!fetch(uri, filter, page, refresh ? FN_LIST_REFRESH : 0))
        return 0;
      refresh = 0;
      if (!pc_find(page)) {     /* asked for a page that does not exist */
        if ((uint32_t) page * CORE_PROTO_PAGE >= dir_total)
          break;
        list_error = FN_ERR_NOT_FOUND;
        return 0;
      }
    }
    rec = pc_find(page);
    if (rec)
      unpack(rec);
  }
  list_stale = 0;
  return 1;
}

/* The whole name of entry `index` (in listing order) of directory `uri`: a page
 * keeps only CORE_NAME_MAX characters of each name, which is enough to show but
 * not to open an entry with a longer one.  0 if it cannot be fetched or does not
 * fit in cap bytes. */
uint8_t core_full_name(const char *uri, const char *filter, uint16_t index, char *out, uint8_t cap)
{
  fn_list_pages_t r;
  uint8_t n;
  uint8_t res;

  rq.start_page = index;                /* one entry per page: page `index` is entry `index` */
  rq.page_size = 1;
  rq.flags = LISTING_FLAGS;
  rq.max_name = 0;
  rq.filter = filter;
  res = fn_file_list_pages(&file_io, uri, &rq, &r);
  rq.page_size = CORE_PROTO_PAGE;       /* back to what fetch() asks for */
  rq.max_name = CORE_NAME_MAX;
  rq.filter = 0;
  if (res != FN_OK || r.page_count == 0 || r.pages[0] == 0)
    return 0;
  n = r.pages[4];                       /* page: count, len; entry: flags, nameLen */
  if (n >= cap)
    return 0;
  memcpy(out, r.pages + 5, n);
  out[n] = 0;
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
    {
      /* the whole URI comes from the slot catalogue, into the page buffer */
      fn_slot_catalog_entry_t e;
      char *full;
      uint16_t n;

      full = (char *) entries;
      list_stale = 1;
      if (fn_slot_catalog_get(&slot_io, drive_slot[u], &e) != FN_OK ||
          !(e.flags & FN_SLOT_CATALOG_ENTRY_VALID)) {
        mount_result = FN_ERR_NOT_FOUND;
        mount_error = FN_DISK_ERR_UNREPORTED;
        if (*failed == 0xFF)
          *failed = u;
        continue;
      }
      n = e.uri_len > CORE_URI_MAX ? CORE_URI_MAX : e.uri_len;
      memcpy(full, e.uri, n);
      full[n] = 0;
      mount_result = fn_disk_mount((uint8_t) (u + 1), full, drive_ro[u],
                                   FN_DISK_TYPE_AUTO, 256, &info);
    }
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
