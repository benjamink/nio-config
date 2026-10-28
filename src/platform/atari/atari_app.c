/* FujiNet NIO Config for the Atari: Hosts, Browse, Drives and Setup tabs on a
 * 40x24 text screen.  atari_core.c holds the data and the FujiNet calls;
 * this file is the screen and the key handling. */
#include "fujinet-nio.h"

#include "atari_core.h"
#include "atari_input.h"
#include "atari_scr.h"

#include <string.h>

#define TAB_HOSTS  0
#define TAB_BROWSE 1
#define TAB_DRIVES 2
#define TAB_SETUP  3
#define TAB_COUNT  4

#define LIST_ROW   3
#define LIST_ROWS  18
#define ROW_FIELD  2
#define ROW_INFO   21
#define ROW_HINT   22
#define ROW_STATUS 23
#define NO_UNIT    0xFF

#define SHFLOK (*(volatile uint8_t *) 0x02BE)

static char uri_buf[CORE_URI_MAX + 24];
static char line[SCR_COLS + 1];
static char status[SCR_COLS];
static char path[CORE_URI_MAX + 1];

static uint8_t tab;
static uint8_t status_error;
static uint8_t cursor_only;     /* the key only moved the cursor within the page */
static uint8_t list_gen;        /* bumped when the directory page is reloaded */

static uint8_t host_sel;
static uint8_t drive_sel;

/* Browse: cursor and first visible row, counted over the ".." row (when
 * path is not the root) and then the directory entries. */
static uint8_t browse_host;
static uint8_t browse_valid;
static uint8_t has_parent;
static uint16_t cur;
static uint16_t top;
static uint8_t target_unit = NO_UNIT;

/* Setup */
static fn_wifi_status_t wifi;
static fn_wifi_config_t wifi_cfg;
static fn_wifi_adapter_info_t wifi_info;
static fn_fuji_info_t fuji_info;
static uint8_t setup_loaded;

static const char *const tab_name[TAB_COUNT] = { "Hosts", "Browse", "Drives", "Setup" };
static const uint8_t tab_col[TAB_COUNT] = { 1, 9, 18, 27 };

/* ---- helpers ------------------------------------------------------------ */

static void say(const char *msg, uint8_t err)
{
  strncpy(status, msg, sizeof(status) - 1);
  status_error = err;
}

static void say_num(const char *prefix, unsigned n)
{
  char tmp[6];
  uint8_t i;
  uint8_t len;

  len = (uint8_t) strlen(prefix);
  memcpy(line, prefix, len);
  i = 0;
  do {
    tmp[i++] = (char) ('0' + n % 10);
    n /= 10;
  } while (n);
  while (i)
    line[len++] = tmp[--i];
  line[len] = 0;
  say(line, 1);
}

static const char *base_name(const char *uri)
{
  const char *p;
  const char *last;

  last = uri;
  for (p = uri; *p; p++)
    if (*p == '/' && p[1])
      last = p + 1;
  return last;
}

/* Show the end of a long string in a field `width` wide. */
static void tail_text(uint8_t row, const char *s)
{
  uint8_t len;

  len = (uint8_t) strlen(s);
  if (len <= 38) {
    scr_text(row, 1, s, 38);
    return;
  }
  scr_putc(row, 1, G_LT);
  scr_text(row, 2, s + len - 37, 37);
}

/* Keep cursor `pos` inside 0..total-1 and the first row `first` showing it. */
static void keep_in_view(uint16_t *pos, uint16_t *first, uint16_t total)
{
  if (total == 0) {
    *pos = 0;
    *first = 0;
    return;
  }
  if (*pos >= total)
    *pos = (uint16_t) (total - 1);
  if (*pos < *first)
    *first = *pos;
  if (*pos >= *first + LIST_ROWS)
    *first = (uint16_t) (*pos - LIST_ROWS + 1);
  if (total <= LIST_ROWS)
    *first = 0;
  else if (*first > total - LIST_ROWS)
    *first = (uint16_t) (total - LIST_ROWS);
}

/* ---- chrome --------------------------------------------------------------- */

static void draw_frame(const char *hint)
{
  uint8_t t;
  uint8_t r;
  uint8_t w;

  /* No full-screen clear: that blanks the display for a few frames.  Every
   * cell is rewritten in place, a row at a time. */
  scr_fill(0, 0, SCR_COLS, ' ');
  scr_fill(1, 0, SCR_COLS, ' ');
  scr_fill(ROW_FIELD, 0, SCR_COLS, ' ');
  scr_fill(ROW_INFO, 0, SCR_COLS, ' ');
  scr_fill(ROW_HINT, 0, SCR_COLS, ' ');
  scr_fill(ROW_STATUS, 0, SCR_COLS, ' ');
  scr_theme(0, T_TITLE);
  scr_theme(1, T_TABS);
  scr_theme(ROW_FIELD, T_FIELD);
  for (r = LIST_ROW; r < LIST_ROW + LIST_ROWS; r++)
    scr_theme(r, T_BODY);
  scr_theme(ROW_INFO, T_INFO);
  scr_theme(ROW_HINT, T_HINT);
  scr_theme(ROW_STATUS, status_error ? T_ERROR : T_STATUS);

  scr_putc(0, 1, G_PEAK_L);
  scr_putc(0, 2, G_PEAK_R);
  scr_text(0, 4, "FujiNet NIO Config", 18);
  {
    const char *net;
    uint8_t n;

    net = wifi.link_state == 2 ? wifi.ip : "no network";
    n = (uint8_t) strlen(net);
    scr_text(0, (uint8_t) (SCR_COLS - 1 - n), net, n);
  }

  for (t = 0; t < TAB_COUNT; t++) {
    w = (uint8_t) strlen(tab_name[t]);
    if (t == tab) {
      scr_putc(1, tab_col[t], (char) (0x80 | ' '));
      scr_text_inv(1, (uint8_t) (tab_col[t] + 1), tab_name[t], w);
      scr_putc(1, (uint8_t) (tab_col[t] + 1 + w), (char) (0x80 | ' '));
    } else {
      scr_text(1, (uint8_t) (tab_col[t] + 1), tab_name[t], w);
    }
  }
  scr_text(ROW_HINT, 1, hint, 38);
  scr_text(ROW_STATUS, 1, status, 38);
}

static void list_row(uint8_t row, uint8_t selected)
{
  scr_theme(row, selected ? T_SEL : T_BODY);
}

/* ---- text entry ------------------------------------------------------------- */

/* Edit buf (cap bytes including the NUL) on the field row.  1 on RET, 0 on ESC. */
static uint8_t edit_line(const char *label, const char *hint, char *buf,
                         uint8_t cap, uint8_t mask)
{
  uint8_t len;
  uint8_t k;
  uint8_t i;
  uint8_t first;

  len = (uint8_t) strlen(buf);
  for (;;) {
    scr_theme(ROW_FIELD, T_EDIT);
    scr_theme(ROW_INFO, T_INFO);
    scr_theme(ROW_HINT, T_HINT);
    scr_text(ROW_INFO, 1, label, 38);
    scr_text(ROW_HINT, 1, hint, 38);
    scr_fill(ROW_FIELD, 0, 40, ' ');
    scr_text(ROW_STATUS, 1, "", 38);
    first = len > 36 ? (uint8_t) (len - 36) : 0;
    for (i = 0; i < 37 && (uint8_t) (first + i) < len; i++)
      scr_putc(ROW_FIELD, (uint8_t) (1 + i), mask ? '*' : buf[first + i]);
    scr_putc(ROW_FIELD, (uint8_t) (1 + len - first), (char) (0x80 | ' '));
    k = input_key();
    if (k == K_RET)
      return 1;
    if (k == K_ESC)
      return 0;
    if (k == K_BS) {
      if (len)
        buf[--len] = 0;
    } else if (k >= 32 && k < 127 && (uint8_t) (len + 1) < cap) {
      buf[len++] = (char) k;
      buf[len] = 0;
    }
  }
}

static uint8_t confirm(const char *question)
{
  uint8_t k;

  scr_theme(ROW_STATUS, T_ERROR);
  scr_text(ROW_STATUS, 1, question, 38);
  k = input_key();
  return k == 'y' || k == 'Y' || k == K_RET;
}

static const core_entry_t *entry_at(uint16_t idx);

/* ---- Long-name animation ------------------------------------------------------------ */

/* When nothing has been pressed for a couple of seconds, the selected row's
 * name, if it does not fit its column, slides to and fro inside that column
 * (the rest of the row stays put), pausing a moment at each end. */
#define MARQ_DELAY 100          /* jiffies idle before it starts (about 2 s) */
#define MARQ_STEP  7            /* jiffies per step */
#define MARQ_HOLD  3            /* steps to rest at each end */

static char marq[CORE_URI_MAX + 1];
static uint8_t marq_state;      /* 0 not tried, 1 animating, 2 fits / nothing to do */
static uint8_t marq_len;
static uint8_t marq_off;
static uint8_t marq_dir;        /* 0 forward, 1 back */
static uint8_t marq_hold;
static uint8_t marq_row;
static uint8_t marq_col;
static uint8_t marq_w;
static uint8_t idle_ok;         /* the main key wait is running: animate */

static void marq_draw(void)
{
  scr_text(marq_row, marq_col, marq + marq_off, marq_w);
}

/* Put the row back as drawn and forget the animation (a key was pressed). */
static void marq_stop(void)
{
  if (marq_state == 1) {
    marq_off = 0;
    marq_draw();
  }
  marq_state = 0;
}

static void marq_start(void)
{
  const core_entry_t *e;

  marq_state = 2;
  marq_off = 0;
  if (tab == TAB_HOSTS && host_count) {
    strcpy(marq, hosts[host_sel]);
    marq_row = (uint8_t) (LIST_ROW + host_sel - top);
    marq_col = 3;
    marq_w = 35;
  } else if (tab == TAB_DRIVES && drive_on[drive_sel]) {
    strcpy(marq, base_name(drive_uri[drive_sel]));
    marq_row = (uint8_t) (LIST_ROW + drive_sel);
    marq_col = 5;
    marq_w = 28;
  } else if (tab == TAB_BROWSE && browse_valid && (e = entry_at(cur)) != 0) {
    strcpy(marq, e->name);
    if (e->flags & CORE_ENTRY_TRUNCATED) {
      /* the page keeps only the start of the name: fetch all of it */
      if (!core_compose(uri_buf, sizeof(uri_buf), hosts[browse_host], path, "") ||
          !core_full_name(uri_buf, (uint16_t) (entry_start + (e - entries)),
                          marq, sizeof(marq)))
        return;
    }
    marq_row = (uint8_t) (LIST_ROW + cur - top);
    marq_col = 1;
    marq_w = 29;
  } else {
    return;
  }
  marq_len = (uint8_t) strlen(marq);
  if (marq_len <= marq_w)
    return;
  marq_state = 1;
  marq_dir = 0;
  marq_hold = MARQ_HOLD;
}

static void idle_tick(void)
{
  if (!idle_ok)
    return;
  if (marq_state == 0)
    marq_start();
  if (marq_state != 1)
    return;
  if (marq_hold) {
    marq_hold--;
    return;
  }
  if (!marq_dir) {
    marq_off++;
    if ((uint8_t) (marq_off + marq_w) >= marq_len) {
      marq_dir = 1;
      marq_hold = MARQ_HOLD;
    }
  } else {
    marq_off--;
    if (!marq_off) {
      marq_dir = 0;
      marq_hold = MARQ_HOLD;
    }
  }
  marq_draw();
}

/* ---- Entry details (size and date) -------------------------------------------------- */

static void put_dec(char **p, uint32_t v, uint8_t width)
{
  char tmp[11];
  uint8_t n;

  n = 0;
  do {
    tmp[n++] = (char) ('0' + (uint8_t) (v % 10));
    v /= 10;
  } while (v);
  while (n < width) {
    tmp[n++] = '0';
  }
  while (n)
    *(*p)++ = tmp[--n];
}

/* "2026-10-28 14:07" from Unix seconds (UTC), civil-from-days. */
static void put_date(char **p, uint32_t t)
{
  uint32_t z;
  uint32_t era;
  uint32_t doe;
  uint32_t yoe;
  uint32_t doy;
  uint32_t mp;
  uint32_t y;
  uint8_t m;
  uint8_t d;
  uint32_t secs;

  secs = t % 86400UL;
  z = t / 86400UL + 719468UL;
  era = z / 146097UL;
  doe = z % 146097UL;
  yoe = (doe - doe / 1460UL + doe / 36524UL - doe / 146096UL) / 365UL;
  y = yoe + era * 400UL;
  doy = doe - (365UL * yoe + yoe / 4UL - yoe / 100UL);
  mp = (5UL * doy + 2UL) / 153UL;
  d = (uint8_t) (doy - (153UL * mp + 2UL) / 5UL + 1UL);
  m = (uint8_t) (mp < 10 ? mp + 3 : mp - 9);
  if (m <= 2)
    y++;
  put_dec(p, y, 4);
  *(*p)++ = '-';
  put_dec(p, m, 2);
  *(*p)++ = '-';
  put_dec(p, d, 2);
  *(*p)++ = ' ';
  put_dec(p, secs / 3600UL, 2);
  *(*p)++ = ':';
  put_dec(p, secs / 60UL % 60UL, 2);
}

/* Show the size and date of the entry under the cursor on the status row,
 * unless a message is showing there. */
static void browse_detail(void)
{
  const core_entry_t *e;
  char *p;

  if (status[0] || tab != TAB_BROWSE || !browse_valid)
    return;
  scr_fill(ROW_STATUS, 0, SCR_COLS, ' ');
  e = entry_at(cur);
  if (!e)
    return;
  p = line;
  if (e->flags & CORE_ENTRY_DIR) {
    strcpy(p, "<DIR>");
    p += 5;
  } else {
    put_dec(&p, e->size, 0);
    strcpy(p, " bytes");
    p += 6;
  }
  if (e->mtime) {
    *p++ = ' ';
    *p++ = ' ';
    put_date(&p, e->mtime);
  }
  *p = 0;
  scr_text(ROW_STATUS, 1, line, 38);
}

/* ---- Hosts tab ------------------------------------------------------------------ */

static void draw_hosts(void)
{
  uint8_t r;
  uint16_t pos;

  draw_frame("RET Browse A Add E Edit D Del OPT Boot");
  pos = host_sel;
  keep_in_view(&pos, &top, host_count);
  scr_text(ROW_FIELD, 1, host_count ? hosts[host_sel] : "", 38);
  for (r = 0; r < LIST_ROWS; r++) {
    uint16_t idx;
    uint8_t row;

    idx = (uint16_t) (top + r);
    row = (uint8_t) (LIST_ROW + r);
    scr_fill(row, 0, SCR_COLS, ' ');
    if (idx >= host_count)
      continue;
    list_row(row, idx == host_sel);
    scr_uint(row, 0, idx + 1, 2);
    scr_text(row, 3, hosts[idx], 35);
  }
  if (!host_count)
    scr_text(LIST_ROW + 1, 3, "No hosts: press A to add one", 36);
  scr_scrollbar(LIST_ROW, LIST_ROWS, top, LIST_ROWS, host_count);
  scr_uint(ROW_INFO, 1, host_count, 2);
  scr_text(ROW_INFO, 4, "of 8 hosts", 12);
}

/* ---- Browse tab ------------------------------------------------------------------- */

static uint16_t browse_known(void)
{
  return (uint16_t) (has_parent + entry_start + entry_count + entry_more);
}

static uint8_t browse_load(uint16_t first)
{
  list_gen++;
  entry_count = 0;
  entry_more = 0;
  entry_start = first;
  if (!browse_valid)
    return 0;
  if (!core_compose(uri_buf, sizeof(uri_buf), hosts[browse_host], path, "")) {
    say("Path is too long", 1);
    return 0;
  }
  if (!core_list(uri_buf, first)) {
    say_num("Browse failed, error ", core_list_error());
    return 0;
  }
  return 1;
}

/* Make sure the rows on screen are in the loaded page of entries. */
static void browse_cover(void)
{
  uint16_t a;

  a = top > has_parent ? (uint16_t) (top - has_parent) : 0;
  if (!list_stale && a >= entry_start &&
      (!entry_more || (uint16_t) (a + LIST_ROWS) <= (uint16_t) (entry_start + entry_count)))
    return;
  (void) browse_load(a > 4 ? (uint16_t) (a - 4) : 0);
}

static void browse_fix(void)
{
  uint16_t known;

  known = browse_known();
  keep_in_view(&cur, &top, known);
  browse_cover();
  known = browse_known();
  keep_in_view(&cur, &top, known);
}

static void browse_reset(void)
{
  has_parent = path[0] ? 1 : 0;
  cur = 0;
  top = 0;
  (void) browse_load(0);
  browse_fix();
}

static void browse_enter_host(uint8_t host)
{
  browse_host = host;
  browse_valid = 1;
  path[0] = 0;
  has_parent = 0;
  browse_reset();
  if (!status_error)
    say("", 0);
}

static void parent_path(void)
{
  uint8_t len;

  len = (uint8_t) strlen(path);
  while (len > 0 && path[len - 1] == '/')
    path[--len] = 0;
  while (len > 0 && path[len - 1] != '/')
    path[--len] = 0;
}

static const core_entry_t *entry_at(uint16_t idx)
{
  if (idx < has_parent)
    return 0;
  idx = (uint16_t) (idx - has_parent);
  if (idx < entry_start || idx >= (uint16_t) (entry_start + entry_count))
    return 0;
  return &entries[idx - entry_start];
}

static void draw_size(uint8_t row, const core_entry_t *e)
{
  if (e->flags & CORE_ENTRY_DIR) {
    scr_text(row, 31, " <DIR>", 6);
  } else if (e->size >= 0x100000UL) {
    scr_uint(row, 31, (e->size + 0xFFFFFUL) >> 20, 5);
    scr_text(row, 36, "M", 1);
  } else {
    scr_uint(row, 31, (e->size + 1023UL) >> 10, 5);
    scr_text(row, 36, "K", 1);
  }
}

static void browse_info(void)
{
  uint16_t known;

  known = browse_known();
  scr_fill(ROW_INFO, 0, SCR_COLS, ' ');
  if (target_unit != NO_UNIT) {
    scr_text(ROW_INFO, 1, "Choose an image for drive D", 27);
    scr_putc(ROW_INFO, 28, (char) ('1' + target_unit));
  } else if (!known) {
    scr_text(ROW_INFO, 1, "Empty directory", 20);
  } else {
    scr_uint(ROW_INFO, 1, cur + 1, 5);
    scr_text(ROW_INFO, 7, "of", 2);
    scr_uint(ROW_INFO, 10, known, 5);
    scr_text(ROW_INFO, 16, entry_more ? "+" : "", 1);
  }
}

static void draw_browse(void)
{
  uint8_t r;
  uint16_t known;

  draw_frame("RET Open BS Up R Reload OPT Boot");
  if (!browse_valid) {
    for (r = 0; r < LIST_ROWS; r++)
      scr_fill((uint8_t) (LIST_ROW + r), 0, SCR_COLS, ' ');
    scr_text(LIST_ROW + 1, 3, "Pick a host on the Hosts tab", 36);
    return;
  }
  known = browse_known();
  if (core_compose(uri_buf, sizeof(uri_buf), hosts[browse_host], path, ""))
    tail_text(ROW_FIELD, uri_buf);
  for (r = 0; r < LIST_ROWS; r++) {
    uint16_t idx;
    uint8_t row;
    const core_entry_t *e;

    idx = (uint16_t) (top + r);
    row = (uint8_t) (LIST_ROW + r);
    scr_fill(row, 0, SCR_COLS, ' ');
    if (idx >= known)
      continue;
    list_row(row, idx == cur);
    if (has_parent && idx == 0) {
      scr_text(row, 1, "..", 28);
      scr_text(row, 31, " <DIR>", 6);
      continue;
    }
    e = entry_at(idx);
    if (!e)
      continue;
    scr_text(row, 1, e->name, 29);
    draw_size(row, e);
  }
  scr_scrollbar(LIST_ROW, LIST_ROWS, top, LIST_ROWS, known);
  browse_info();
  browse_detail();
}

/* ---- Drives --------------------------------------------------------------------------- */

static void assign_drive(uint8_t unit, const char *uri, uint8_t readonly)
{
  if (!core_put_slot(unit, uri, readonly)) {
    say("Unable to save drive slot", 1);
    return;
  }
  strncpy(drive_uri[unit], uri, CORE_URI_MAX);
  drive_uri[unit][CORE_URI_MAX] = 0;
  drive_on[unit] = 1;
  drive_ro[unit] = readonly;
  drive_slot[unit] = unit;
  if (!core_save_drives()) {
    say("Unable to save drive map", 1);
    return;
  }
  memcpy(line, "D? <- ", 6);
  line[1] = (char) ('1' + unit);
  strncpy(line + 6, base_name(uri), 30);
  line[36] = 0;
  say(line, 0);
}

static void draw_drives(void)
{
  uint8_t u;

  draw_frame("RET Pick X Eject R Read-only OPT Boot");
  if (drive_on[drive_sel])
    tail_text(ROW_FIELD, drive_uri[drive_sel]);
  for (u = 0; u < CORE_UNITS; u++) {
    uint8_t row;

    row = (uint8_t) (LIST_ROW + u);
    scr_fill(row, 0, SCR_COLS, ' ');
    list_row(row, u == drive_sel);
    scr_putc(row, 1, 'D');
    scr_putc(row, 2, (char) ('1' + u));
    scr_putc(row, 3, ':');
    if (drive_on[u]) {
      scr_text(row, 5, base_name(drive_uri[u]), 28);
      scr_text(row, 34, drive_ro[u] ? "RO" : "RW", 2);
    } else {
      scr_text(row, 5, "(empty)", 28);
    }
  }
  for (u = CORE_UNITS; u < LIST_ROWS; u++)
    scr_fill((uint8_t) (LIST_ROW + u), 0, SCR_COLS, ' ');
  scr_text(ROW_INFO, 1, "OPTION boots with these drives mounted", 38);
}

/* ---- Setup ----------------------------------------------------------------------------- */

static void setup_refresh(void)
{
  memset(&wifi, 0, sizeof(wifi));
  memset(&wifi_cfg, 0, sizeof(wifi_cfg));
  memset(&wifi_info, 0, sizeof(wifi_info));
  memset(&fuji_info, 0, sizeof(fuji_info));
  (void) fn_wifi_get_status(&wifi);
  (void) fn_wifi_get_config(&wifi_cfg);
  (void) fn_wifi_get_adapter_info(&wifi_info);
  (void) fn_fuji_get_info(&fuji_info);
  setup_loaded = 1;
}

static void setup_row(uint8_t n, const char *label, const char *value)
{
  uint8_t row;

  row = (uint8_t) (LIST_ROW + n);
  scr_text(row, 1, label, 12);
  scr_text(row, 14, value, 24);
}

static const char *link_text(void)
{
  switch (wifi.link_state) {
  case 0: return "Disconnected";
  case 1: return "Connecting";
  case 2: return "Connected";
  default: return "Failed";
  }
}

static void draw_setup(void)
{
  static const char hex[] = "0123456789abcdef";
  char *mac;
  uint8_t i;

  mac = line;
  draw_frame("RET Join Wi-Fi R Refresh OPT Boot");
  for (i = 0; i < LIST_ROWS; i++)
    scr_fill((uint8_t) (LIST_ROW + i), 0, SCR_COLS, ' ');
  list_row(LIST_ROW, 1);
  setup_row(0, "Wi-Fi", wifi_cfg.ssid[0] ? wifi_cfg.ssid : "(not set)");
  setup_row(1, "Link", link_text());
  setup_row(2, "IP address", wifi.link_state == 2 ? wifi.ip : "-");
  setup_row(3, "Gateway", wifi.link_state == 2 ? wifi.gateway : "-");
  setup_row(4, "DNS", wifi.link_state == 2 ? wifi.dns : "-");
  if (wifi_info.mac.valid) {
    for (i = 0; i < 6; i++) {
      mac[i * 3] = hex[wifi_info.mac.bytes[i] >> 4];
      mac[i * 3 + 1] = hex[wifi_info.mac.bytes[i] & 15];
      mac[i * 3 + 2] = ':';
    }
    mac[17] = 0;
  } else {
    strcpy(mac, "unknown");
  }
  setup_row(5, "MAC", mac);
  setup_row(7, "Firmware", fuji_info.firmware[0] ? fuji_info.firmware : "unknown");
  setup_row(8, "Build", fuji_info.profile[0] ? fuji_info.profile : "unknown");
}

/* Scan for networks and let the user pick one; 0 if none was chosen. */
#define SCAN_MAX 8

static uint8_t pick_network(char *ssid)
{
  fn_wifi_scan_record_t *recs;
  uint8_t *wire;
  uint8_t count;
  uint8_t more;
  uint8_t sel;
  uint8_t i;
  uint8_t k;

  recs = (fn_wifi_scan_record_t *) core_scratch();
  wire = (uint8_t *) (recs + SCAN_MAX);
  draw_frame("");
  scr_text(ROW_STATUS, 1, "Scanning for networks...", 38);
  count = 0;
  if (fn_wifi_scan(0, SCAN_MAX, recs, SCAN_MAX, &count, &more, wire,
                   FN_WIFI_SCAN_RESPONSE_HEADER_SIZE +
                   SCAN_MAX * FN_WIFI_SCAN_RECORD_WIRE_MAX) != FN_OK)
    count = 0;
  if (!count)
    return 0;
  sel = 0;
  for (;;) {
    draw_frame("RET Choose M Type name ESC Cancel");
    scr_text(ROW_STATUS, 1, "Wi-Fi networks in range", 38);
    for (i = 0; i < count; i++) {
      uint8_t row;

      row = (uint8_t) (LIST_ROW + i);
      list_row(row, i == sel);
      scr_text(row, 1, recs[i].ssid, 28);
      scr_text(row, 31, recs[i].auth ? "secured" : "open", 7);
    }
    k = input_key();
    if (k >= 'A' && k <= 'Z')
      k = (uint8_t) (k + 32);
    if (k == K_ESC)
      return 0xFF;
    if (k == 'm')
      return 0;
    if (k == K_RET) {
      strcpy(ssid, recs[sel].ssid);
      return 1;
    }
    if ((k == K_UP || k == '-') && sel)
      sel--;
    else if ((k == K_DOWN || k == '=') && (uint8_t) (sel + 1) < count)
      sel++;
  }
}

static void join_wifi(void)
{
  char *ssid;
  char *pass;
  fn_wifi_config_update_t up;
  uint8_t picked;

  /* The page buffer holds the text being edited (and the scan list). */
  ssid = (char *) core_scratch() + 700;     /* past the scan records and their wire buffer */
  pass = ssid + FN_WIFI_MAX_SSID + 1;
  strcpy(ssid, wifi_cfg.ssid);
  pass[0] = 0;
  picked = pick_network(ssid);
  if (picked == 0xFF)
    return;
  if (!picked && !edit_line("Wi-Fi network name (SSID)", "RET Next ESC Cancel",
                            ssid, FN_WIFI_MAX_SSID + 1, 0))
    return;
  if (!edit_line("Password (blank for open)", "RET Join ESC Cancel", pass,
                 FN_WIFI_MAX_PASSWORD + 1, 1))
    return;
  up.fields = (uint8_t) (FN_WIFI_SET_ENABLED | FN_WIFI_SET_SSID |
                         FN_WIFI_SET_PASSWORD | FN_WIFI_SET_PERSIST);
  up.enabled = 1;
  up.ssid = ssid;
  up.bssid = 0;
  up.password = pass;
  if (fn_wifi_set_config(&up) != FN_OK) {
    say("Wi-Fi settings were refused", 1);
    return;
  }
  say("Wi-Fi saved", 0);
  setup_refresh();
}

/* ---- Boot -------------------------------------------------------------------------------- */

static void boot_now(void)
{
  uint8_t u;
  uint8_t failed;

  for (u = 0; u < CORE_UNITS && !drive_on[u]; u++)
    ;
  if (u == CORE_UNITS) {
    say("Assign an image to a drive first", 1);
    return;
  }
  scr_theme(ROW_STATUS, T_STATUS);
  scr_text(ROW_STATUS, 1, "Mounting drives...", 38);
  if (!core_mount_all(&failed) || failed != 0xFF) {
    say(core_mount_error(), 1);
    return;
  }
  scr_text(ROW_STATUS, 1, "Booting...", 38);
  scr_coldstart();
}

/* ---- Mounting a file ---------------------------------------------------------------------- */

static void open_file(const char *name)
{
  uint8_t readonly;
  uint8_t r;
  uint8_t k;

  if (!core_compose(uri_buf, sizeof(uri_buf), hosts[browse_host], path, name) ||
      strlen(uri_buf) > CORE_URI_MAX) {
    say("URI is longer than 79 characters", 1);
    return;
  }
  if (target_unit != NO_UNIT) {
    assign_drive(target_unit, uri_buf, 0);
    drive_sel = target_unit;
    target_unit = NO_UNIT;
    tab = TAB_DRIVES;
    return;
  }
  readonly = 0;
  for (;;) {
    for (r = 7; r < 15; r++) {
      scr_theme(r, T_DIALOG);
      scr_fill(r, 0, 40, ' ');
    }
    scr_text(7, 2, "Assign to a drive", 36);
    scr_text(8, 2, name, 36);
    scr_text(10, 2, "Press 1-8 to assign to D1-D8", 36);
    scr_text(11, 2, readonly ? "R   Write protect: ON" : "R   Write protect: OFF", 36);
    scr_text(13, 2, "ESC cancel", 36);
    k = input_key();
    if (k == K_ESC)
      return;
    if (k == 'r' || k == 'R') {
      readonly = (uint8_t) !readonly;
    } else if (k >= '1' && k <= '8') {
      assign_drive((uint8_t) (k - '1'), uri_buf, readonly);
      drive_sel = (uint8_t) (k - '1');
      return;
    }
  }
}

/* ---- Keys ---------------------------------------------------------------------------------- */

static uint8_t move_cursor(uint8_t k, uint16_t *pos, uint16_t total)
{
  if (!total)
    return 0;
  if (k == K_UP || k == '-') {
    if (*pos > 0)
      (*pos)--;
  } else if (k == K_DOWN || k == '=') {
    if ((uint16_t) (*pos + 1) < total)
      (*pos)++;
  } else if (k == ' ') {
    *pos = (uint16_t) (*pos + LIST_ROWS);
    if (*pos >= total)
      *pos = (uint16_t) (total - 1);
  } else if (k == 'b') {
    *pos = *pos > LIST_ROWS ? (uint16_t) (*pos - LIST_ROWS) : 0;
  } else {
    return 0;
  }
  return 1;
}

static void key_hosts(uint8_t k)
{
  uint16_t pos;

  pos = host_sel;
  if (move_cursor(k, &pos, host_count)) {
    host_sel = (uint8_t) pos;
    cursor_only = 1;
    return;
  }
  if (k == K_RET && host_count) {
    browse_enter_host(host_sel);
    tab = TAB_BROWSE;
  } else if (k == 'a' && host_count < CORE_MAX_HOSTS) {
    uri_buf[0] = 0;
    if (edit_line("New host or URI", "RET Add ESC Cancel", uri_buf,
                  CORE_URI_MAX + 1, 0) && uri_buf[0]) {
      strcpy(hosts[host_count], uri_buf);
      host_sel = host_count++;
      say(core_save_hosts() ? "Host added" : "Unable to save hosts", 0);
    }
  } else if (k == 'e' && host_count) {
    strcpy(uri_buf, hosts[host_sel]);
    if (edit_line("Edit host or URI", "RET Save ESC Cancel", uri_buf,
                  CORE_URI_MAX + 1, 0) && uri_buf[0]) {
      strcpy(hosts[host_sel], uri_buf);
      say(core_save_hosts() ? "Host saved" : "Unable to save hosts", 0);
    }
  } else if (k == 'd' && host_count) {
    if (confirm("Delete this host? Y/N")) {
      uint8_t i;

      for (i = host_sel; (uint8_t) (i + 1) < host_count; i++)
        strcpy(hosts[i], hosts[i + 1]);
      host_count--;
      if (host_sel >= host_count && host_sel)
        host_sel--;
      say(core_save_hosts() ? "Host deleted" : "Unable to save hosts", 0);
    }
  }
}

static void key_browse(uint8_t k)
{
  const core_entry_t *e;
  uint8_t len;
  uint8_t nlen;
  uint8_t isdir;
  char *fullname;

  if (!browse_valid) {
    if (k == K_RET)
      tab = TAB_HOSTS;
    return;
  }
  if (move_cursor(k, &cur, browse_known())) {
    browse_fix();
    cursor_only = 1;
    return;
  }
  if (k == K_BS || k == 'u' || (k == K_RET && has_parent && cur == 0)) {
    if (path[0]) {
      parent_path();
      browse_reset();
    }
    return;
  }
  if (k == 'r') {
    (void) browse_load(entry_start);
    browse_fix();
    return;
  }
  if (k != K_RET)
    return;
  e = entry_at(cur);
  if (!e)
    return;
  /* A page keeps only the start of a long name: ask for the whole one. */
  isdir = (uint8_t) (e->flags & CORE_ENTRY_DIR);
  fullname = (char *) core_scratch();   /* the page buffer: reloaded afterwards */
  if (e->flags & CORE_ENTRY_TRUNCATED) {
    if (!core_compose(uri_buf, sizeof(uri_buf), hosts[browse_host], path, "") ||
        !core_full_name(uri_buf, (uint16_t) (entry_start + (e - entries)),
                        fullname, CORE_URI_MAX + 1)) {
      say("Name is longer than 79 characters", 1);
      return;
    }
  } else {
    memmove(fullname, e->name, strlen(e->name) + 1);
  }
  if (isdir) {
    len = (uint8_t) strlen(path);
    nlen = (uint8_t) strlen(fullname);
    if ((uint16_t) (len + nlen + 2) > CORE_URI_MAX) {
      say("Path is longer than 79 characters", 1);
      return;
    }
    strcpy(path + len, fullname);
    path[len + nlen] = '/';
    path[len + nlen + 1] = 0;
    browse_reset();
  } else {
    open_file(fullname);
    if (tab == TAB_BROWSE)
      browse_fix();             /* the page buffer was borrowed */
  }
}

static void key_drives(uint8_t k)
{
  uint16_t pos;

  pos = drive_sel;
  if (move_cursor(k, &pos, CORE_UNITS)) {
    drive_sel = (uint8_t) pos;
    cursor_only = 1;
    return;
  }
  if (k == 'x' || k == K_BS) {
    drive_on[drive_sel] = 0;
    say(core_save_drives() ? "Drive cleared" : "Unable to save drive map", 0);
  } else if (k == 'r') {
    if (drive_on[drive_sel]) {
      drive_ro[drive_sel] ^= 1;
      say(core_save_drives() ? "Write protect changed" : "Unable to save drive map", 0);
    }
  } else if (k == K_RET) {
    target_unit = drive_sel;
    if (browse_valid) {
      tab = TAB_BROWSE;
    } else {
      tab = TAB_HOSTS;
      say("Pick a host, then choose the image", 0);
    }
  } else if (k >= '1' && k <= '8') {
    drive_sel = (uint8_t) (k - '1');
  }
}

static void key_setup(uint8_t k)
{
  if (k == 'r') {
    setup_refresh();
    say("Refreshed", 0);
  } else if (k == K_RET) {
    join_wifi();
  }
}

/* The cursor moved but the page did not scroll: only the selection bar, the
 * field and the info line change, so redraw just those. */
static void update_selection(void)
{
  uint8_t r;
  uint16_t idx;

  for (r = 0; r < LIST_ROWS; r++) {
    idx = (uint16_t) (top + r);
    if (tab == TAB_HOSTS && idx < host_count)
      list_row((uint8_t) (LIST_ROW + r), idx == host_sel);
    else if (tab == TAB_BROWSE && idx < browse_known())
      list_row((uint8_t) (LIST_ROW + r), idx == cur);
    else if (tab == TAB_DRIVES && idx < CORE_UNITS)
      list_row((uint8_t) (LIST_ROW + r), idx == drive_sel);
  }
  scr_fill(ROW_FIELD, 0, SCR_COLS, ' ');
  if (tab == TAB_HOSTS) {
    scr_text(ROW_FIELD, 1, host_count ? hosts[host_sel] : "", 38);
  } else if (tab == TAB_DRIVES) {
    if (drive_on[drive_sel])
      tail_text(ROW_FIELD, drive_uri[drive_sel]);
  } else {
    if (core_compose(uri_buf, sizeof(uri_buf), hosts[browse_host], path, ""))
      tail_text(ROW_FIELD, uri_buf);
    browse_info();
    browse_detail();
  }
}

static void set_tab(uint8_t t)
{
  tab = t;
  top = 0;
  if (t == TAB_SETUP && !setup_loaded)
    setup_refresh();
  if (t == TAB_BROWSE && browse_valid)
    browse_fix();
}

static void run(void)
{
  uint8_t k;
  uint8_t redraw;
  uint8_t old_tab;
  uint16_t old_top;
  uint8_t old_gen;

  setup_refresh();
  say("Ready", 0);
  input_set_idle(idle_tick, MARQ_DELAY, MARQ_STEP);
  redraw = 1;
  for (;;) {
    if (redraw) {
      switch (tab) {
      case TAB_HOSTS:  draw_hosts();  break;
      case TAB_BROWSE: draw_browse(); break;
      case TAB_DRIVES: draw_drives(); break;
      default:         draw_setup();  break;
      }
    }
    redraw = 1;
    idle_ok = 1;
    k = input_key();
    idle_ok = 0;
    marq_stop();
    if (k == K_BOOT) {
      boot_now();
      continue;
    }
    if (k == K_TAB) {
      set_tab((uint8_t) ((tab + 1) % TAB_COUNT));
      continue;
    }
    /* In a list, right opens the item and left goes up a level (so the
     * joystick can browse); the tabs change with TAB or SELECT. */
    if (k == K_RIGHT && tab != TAB_SETUP)
      k = K_RET;
    else if (k == K_LEFT && tab == TAB_BROWSE)
      k = K_BS;
    else if (k == K_LEFT || k == K_RIGHT)
      continue;
    if (k >= 'A' && k <= 'Z')
      k = (uint8_t) (k + 32);
    status_error = 0;
    cursor_only = 0;
    old_tab = tab;
    old_top = top;
    old_gen = list_gen;
    switch (tab) {
    case TAB_HOSTS:  key_hosts(k);  break;
    case TAB_BROWSE: key_browse(k); break;
    case TAB_DRIVES: key_drives(k); break;
    default:         key_setup(k);  break;
    }
    if (cursor_only && tab == old_tab && top == old_top && list_gen == old_gen) {
      /* keep_in_view may still scroll on the next draw; here it did not */
      status[0] = 0;            /* the size/date of the new entry replaces it */
      status_error = 0;
      scr_theme(ROW_STATUS, T_STATUS);
      scr_fill(ROW_STATUS, 0, SCR_COLS, ' ');
      update_selection();
      redraw = 0;
    }
  }
}

int main(void)
{
  uint8_t failed;

  SHFLOK = 0;                   /* lower case by default: URIs are case sensitive */
  scr_init();
  scr_theme(ROW_STATUS, T_STATUS);
  scr_text(ROW_STATUS, 1, "Starting FujiNet...", 38);

  failed = 0;
  if (fn_init() != FN_OK)
    failed = 1;
  scr_sio_hook();               /* after fn_init, which sets the plain SIO vector */
  if (!failed && !fn_is_ready())
    failed = 2;
  if (!failed && !core_load())
    failed = 3;
  if (failed) {
    scr_theme(ROW_STATUS, T_ERROR);
    scr_text(ROW_STATUS, 1, failed == 1 ? "FujiNet is not responding" :
             failed == 2 ? "FujiNet is not ready" : "Cannot read settings", 29);
    scr_uint(ROW_STATUS, 30, core_error, 3);
    scr_text(ROW_STATUS, 34, "sio", 3);
    scr_uint(ROW_STATUS, 36, fn_atari_last_sio_status(), 3);
    for (;;)
      ;
  }
  input_settle();
  run();
  return 0;
}
