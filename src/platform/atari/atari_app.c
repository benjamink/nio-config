/* FujiNet NIO Config for the Atari: Hosts, Browse, Drives and Setup tabs on a
 * 40x24 text screen.  atari_core.c holds the data and the FujiNet calls;
 * this file is the screen and the key handling. */
#include "fujinet-nio.h"

#include "atari_core.h"
#include "atari_input.h"
#include "atari_scr.h"

#include <string.h>

/* Shared buffers and who may use them (nothing here is held across a key
 * wait except where noted):
 *   uri_buf  (CORE_URI_MAX+1)  a composed URI: the listing request, the field
 *            on screen, the mount dialog (held while the dialog waits for a
 *            key) and the text of a host being edited.  The idle animation is
 *            off during dialogs and edits, so it never competes.
 *   marq     the full name being animated; only idle_tick() touches it.
 *   entries  (core_scratch)  the directory page, borrowed for host text, a
 *            name being opened, and the Wi-Fi scan and SSID/password; each
 *            borrower marks the page stale so the next draw reloads it.
 *   iobuf    (atari_core.c)  every FujiNet request/response; never held.
 *   line     one row of text; the detail line and the MAC.
 */
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

/* The long buffers live in low memory (see makefiles/cc65-atari-nio.cfg), which
 * nothing clears: path is set in main(), the others are written before read. */
#pragma bss-name (push, "LOWMEM")
static char uri_buf[CORE_URI_MAX + 1];
static char path[CORE_URI_MAX + 1];
static char marq[CORE_URI_MAX + 1];
static char filter[41];       /* no initial value: low memory */
#pragma bss-name (pop)
static char line[SCR_COLS + 1];
static char status[SCR_COLS];
static fn_wifi_status_t wifi;

static uint8_t tab;
#ifdef DRAW_TIMING
uint8_t dbg_draw;               /* jiffies the last full draw took (dev builds) */
uint8_t dbg_move;               /* jiffies the last cursor-move update took */
#endif
static uint8_t status_error;
static uint8_t cursor_only;     /* the key only moved the cursor within the page */
static uint8_t list_gen;        /* bumped when the directory page is reloaded */

static uint8_t host_sel;
static uint8_t drive_sel;

/* Browse: the directory is shown a page (LIST_ROWS entries) at a time. */
static uint8_t browse_host;
static uint8_t browse_valid;
static uint16_t bpage;          /* screen page of the directory being shown (0-based) */
static uint16_t npages;         /* screen pages in the directory */
static uint8_t brow;            /* cursor row within the page */
static uint16_t top;
static uint8_t target_unit = NO_UNIT;

/* Setup */
/* Setup data shares the directory page: the two views are never active at
 * once. Scanning also borrows it, so setup is refreshed after a join dialog. */
typedef struct { fn_wifi_config_t cfg; fn_wifi_adapter_info_t info; fn_fuji_info_t fuji; } setup_data_t;
#define setup_data (*(setup_data_t *) entries)
#define wifi_cfg setup_data.cfg
#define wifi_info setup_data.info
#define fuji_info setup_data.fuji

static const char *const tab_name[TAB_COUNT] = { "Hosts", "Browse", "Drives", "Setup" };
static const uint8_t tab_col[TAB_COUNT] = { 1, 9, 18, 27 };

/* Setup lives in its own expanded-memory bank in the banked build.  Calls
 * from resident code go through assembly gates; calls within Setup stay local. */
#ifdef CONFIG_NIO_BANKED
#define SETUP_IMPL(name) name##_impl
#define SETUP_SCOPE
void setup_refresh(void);
void draw_setup(void);
void key_setup(uint8_t k);
#else
#define SETUP_IMPL(name) name
#define SETUP_SCOPE static
#endif

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

const char *__fastcall__ base_name(const char *uri);

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
extern uint16_t *view_pos, *view_first;
extern uint16_t view_total;
#pragma zpsym("view_pos")
#pragma zpsym("view_first")
#pragma zpsym("view_total")
void view_run(void);
#define keep_in_view(p, f, t) do { view_pos = (p); view_first = (f); view_total = (t); view_run(); } while (0)

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

static uint8_t sel_row = 0xFF;  /* screen row currently drawn as the selection bar */

static void list_row(uint8_t row, uint8_t selected)
{
  scr_theme(row, selected ? T_SEL : T_BODY);
  if (selected)
    sel_row = row;
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

/* ---- Long-name animation ------------------------------------------------------------ */

/* When nothing has been pressed for a couple of seconds, the selected row's
 * name, if it does not fit its column, slides to and fro inside that column
 * (the rest of the row stays put), pausing a moment at each end. */
#define MARQ_DELAY 100          /* jiffies idle before it starts (about 2 s) */
#define MARQ_STEP  7            /* jiffies per step */
#define MARQ_HOLD  3            /* steps to rest at each end */

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
    strcpy(marq, host_ptr[host_sel]);
    marq_row = (uint8_t) (LIST_ROW + host_sel - top);
    marq_col = 3;
    marq_w = 35;
  } else if (tab == TAB_DRIVES && drive_on[drive_sel]) {
    strcpy(marq, base_name(drive_uri[drive_sel]));
    marq_row = (uint8_t) (LIST_ROW + drive_sel);
    marq_col = 5;
    marq_w = 28;
  } else if (tab == TAB_BROWSE && browse_valid && brow < entry_count) {
    e = &entries[brow];
    strcpy(marq, e->name);
    if (e->flags & CORE_ENTRY_TRUNCATED) {
      /* the page keeps only the start of the name: fetch all of it */
      if (!core_compose(uri_buf, sizeof(uri_buf), host_ptr[browse_host], path, "") ||
          !core_full_name(uri_buf, filter, (uint16_t) (bpage * LIST_ROWS + brow),
                          marq, sizeof(marq)))
        return;
    }
    marq_row = (uint8_t) (LIST_ROW + brow);
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

/* Decimal text.  32-bit division is slow on a 6502, so numbers are split into
 * 16-bit pieces and the 32-bit division happens at most a couple of times. */
static void put_dec16(char **p, uint16_t v, uint8_t width)
{
  char *q;

  q = *p;
  scr_dec16(q, v, width);
  *p = q;
}

static void put_dec32(char **p, uint32_t v)
{
  if (v < 65536UL) {
    put_dec16(p, (uint16_t) v, 0);
  } else {
    put_dec32(p, v / 10000UL);
    put_dec16(p, (uint16_t) (v % 10000UL), 4);
  }
}

/* "2026-10-28 14:07" (UTC) from Unix seconds.  Whole days since 1970 fit 16
 * bits, so only the split into days and minutes uses 32-bit division. */
#pragma code-name(push, "TAB_BROWSE")
static void put_date(char **p, uint32_t t)
{
  static const uint8_t mdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  uint16_t days;
  uint16_t mins;
  uint16_t y;
  uint8_t m;
  uint8_t leap;
  uint8_t dim;

  days = (uint16_t) (t / 86400UL);
  mins = (uint16_t) ((t - (uint32_t) days * 86400UL) / 60UL);
  /* 1970-2099: every fourth year is a leap year, so count whole 4-year blocks
   * (1461 days) and then at most three single years: 1970, 71 (365 days), 72
   * (366), 73 (365). */
  y = (uint16_t) (1970 + 4 * (days / 1461));
  days = (uint16_t) (days % 1461);
  leap = 0;
  if (days >= 365) {
    days = (uint16_t) (days - 365);
    y++;
    if (days >= 365) {
      days = (uint16_t) (days - 365);
      y++;
      leap = 1;                 /* the year 1972 + 4k */
      if (days >= 366) {
        days = (uint16_t) (days - 366);
        y++;
        leap = 0;
      }
    }
  }
  for (m = 0; m < 12; m++) {
    dim = (uint8_t) (mdays[m] + (m == 1 && leap));
    if (days < dim)
      break;
    days = (uint16_t) (days - dim);
  }
  put_dec16(p, y, 4);
  *(*p)++ = '-';
  put_dec16(p, (uint16_t) (m + 1), 2);
  *(*p)++ = '-';
  put_dec16(p, (uint16_t) (days + 1), 2);
  *(*p)++ = ' ';
  put_dec16(p, (uint16_t) (mins / 60), 2);
  *(*p)++ = ':';
  put_dec16(p, (uint16_t) (mins % 60), 2);
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
  if (brow >= entry_count)
    return;
  e = &entries[brow];
  /* size (or <DIR>) flush left, date flush right so it never shifts */
  p = line;
  if (e->flags & CORE_ENTRY_DIR) {
    strcpy(p, "<DIR>");
    p += 5;
  } else {
    put_dec32(&p, e->size);
    strcpy(p, " bytes");
    p += 6;
  }
  *p = 0;
  scr_text(ROW_STATUS, 1, line, 21);
  if (e->mtime) {
    p = line;
    put_date(&p, e->mtime);
    *p = 0;
    scr_text(ROW_STATUS, (uint8_t) (SCR_COLS - 1 - (p - line)), line, (uint8_t) (p - line));
  }
}

#pragma code-name(pop)

/* ---- Hosts tab ------------------------------------------------------------------ */

/* List row r (0..LIST_ROWS-1) of the Hosts tab, for the current `top`. */
#pragma code-name(push, "TAB_HOSTS")
static void hosts_row(uint8_t r)
{
  uint16_t idx;
  uint8_t row;

  idx = (uint16_t) (top + r);
  row = (uint8_t) (LIST_ROW + r);
  scr_fill(row, 0, SCR_COLS, ' ');
  if (idx >= host_count)
    return;
  list_row(row, idx == host_sel);
  scr_uint(row, 0, (unsigned) (idx + 1), 2);
  scr_text(row, 3, host_ptr[idx], 35);
}
#pragma code-name(pop)

#pragma code-name(push, "TAB_HOSTS")
static void draw_hosts(void)
{
  uint8_t r;
  uint16_t pos;

  draw_frame("RET Browse A Add E Edit D Del OPT Boot");
  pos = host_sel;
  keep_in_view(&pos, &top, host_count);
  scr_text(ROW_FIELD, 1, host_count ? host_ptr[host_sel] : "", 38);
  for (r = 0; r < LIST_ROWS; r++)
    hosts_row(r);
  if (!host_count)
    scr_text(LIST_ROW + 1, 3, "No hosts: press A to add one", 36);
  scr_scrollbar(LIST_ROW, LIST_ROWS, top, LIST_ROWS, host_count);
  scr_uint(ROW_INFO, 1, host_count, 2);
  scr_text(ROW_INFO, 4, host_count == 1 ? "host" : "hosts", 5);
}
#pragma code-name(pop)

/* ---- Browse tab ------------------------------------------------------------------- */

/* What went wrong with a directory request, in words. */
#pragma code-name(push, "TAB_BROWSE")
static void say_list_error(void)
{
  switch (core_list_error()) {
  case FN_ERR_TIMEOUT:   say("The server took too long to answer", 1); break;
  case FN_ERR_NOT_FOUND: say("Folder not found", 1); break;
  case FN_ERR_IO:        say("Could not read the folder", 1); break;
  default:               say_num("Browse failed, error ", core_list_error()); break;
  }
}
#pragma code-name(pop)

/* Show screen page `sp` of the directory with the cursor on row `row`.  Only a
 * page the device has not sent yet costs a request (and a "Reading" message);
 * on failure the page on screen is left as it was. */
#pragma code-name(push, "TAB_BROWSE")
static uint8_t browse_goto(uint16_t sp, uint8_t row, uint8_t refresh)
{
  uint8_t tries;

  if (!browse_valid)
    return 0;
  if (!core_compose(uri_buf, sizeof(uri_buf), host_ptr[browse_host], path, "")) {
    say("Path is too long", 1);
    return 0;
  }
  for (tries = 0; tries < 2; tries++) {
    list_gen++;
    if (!core_dir_have(sp) || refresh) {
      scr_fill(ROW_STATUS, 0, SCR_COLS, ' ');
      scr_theme(ROW_STATUS, T_STATUS);
      scr_text(ROW_STATUS, 1, "Reading directory...", 38);
      scr_present();
    }
    if (!core_dir_page(uri_buf, filter, sp, refresh)) {
      say_list_error();
      return 0;
    }
    refresh = 0;
    if (entry_count || !sp || dir_total == 0xFFFFFFFFUL || !dir_total)
      break;
    sp = (uint16_t) ((dir_total - 1) / LIST_ROWS);   /* the directory got shorter */
  }
  bpage = sp;
  npages = dir_total == 0xFFFFFFFFUL ? 1 :
           (uint16_t) ((dir_total + LIST_ROWS - 1) / LIST_ROWS);
  brow = row < entry_count ? row : (uint8_t) (entry_count ? entry_count - 1 : 0);
  return 1;
}
#pragma code-name(pop)

/* A new directory: forget everything the device sent for the last one. */
#pragma code-name(push, "TAB_BROWSE")
static void browse_reset(void)
{
  core_dir_reset();
  bpage = 0;
  npages = 0;
  brow = 0;
  (void) browse_goto(0, 0, 0);
}
#pragma code-name(pop)

#pragma code-name(push, "TAB_BROWSE")
static void browse_enter_host(uint8_t host)
{
  browse_host = host;
  browse_valid = 1;
  path[0] = 0;
  status[0] = 0;
  filter[0] = 0;
  browse_reset();
  if (!status_error)
    say("", 0);
}
#pragma code-name(pop)

#pragma code-name(push, "TAB_BROWSE")
static void parent_path(void)
{
  uint8_t len;

  len = (uint8_t) strlen(path);
  while (len > 0 && path[len - 1] == '/')
    path[--len] = 0;
  while (len > 0 && path[len - 1] != '/')
    path[--len] = 0;
}
#pragma code-name(pop)

#pragma code-name(push, "TAB_BROWSE")
static void draw_size(uint8_t row, const core_entry_t *e)
{
  if (e->flags & CORE_ENTRY_DIR) {
    scr_text(row, 31, " <DIR>", 6);
    return;
  }
  scr_uint(row, 31, e->disp, 5);
  scr_text(row, 36, (e->flags & CORE_ENTRY_MB) ? "M" : "K", 1);
}
#pragma code-name(pop)

#pragma code-name(push, "TAB_BROWSE")
static void browse_info(void)
{
  unsigned total;

  scr_fill(ROW_INFO, 0, SCR_COLS, ' ');
  if (target_unit != NO_UNIT) {
    scr_text(ROW_INFO, 1, "Choose an image for drive D", 27);
    scr_putc(ROW_INFO, 28, (char) ('1' + target_unit));
  } else if (!entry_count) {
    scr_text(ROW_INFO, 1, filter[0] ? "No matching entries" : "Empty directory", 20);
  } else {
    total = dir_total > 65535UL ? 65535U : (unsigned) dir_total;
    scr_text(ROW_INFO, 1, "Page", 4);
    scr_uint(ROW_INFO, 6, (unsigned) (bpage + 1), 4);
    scr_text(ROW_INFO, 11, "of", 2);
    scr_uint(ROW_INFO, 14, npages, 4);
    scr_uint(ROW_INFO, 24, total, 5);
    scr_text(ROW_INFO, 30, total == 1 ? "entry" : "entries", 7);
  }
  if (filter[0]) {
    scr_text(ROW_INFO, 1, "Filter:", 7);
    scr_text(ROW_INFO, 9, filter, 29);
  }
}
#pragma code-name(pop)

/* List row r of the Browse tab. */
#pragma code-name(push, "TAB_BROWSE")
static void browse_row(uint8_t r)
{
  uint8_t row;
  const core_entry_t *e;

  row = (uint8_t) (LIST_ROW + r);
  scr_fill(row, 0, SCR_COLS, ' ');
  if (r >= entry_count)
    return;
  list_row(row, r == brow);
  e = &entries[r];
  scr_text(row, 1, e->name, 29);
  draw_size(row, e);
}
#pragma code-name(pop)

#pragma code-name(push, "TAB_BROWSE")
static void draw_browse(void)
{
  uint8_t r;

  draw_frame("RET Open </> BS Up F Filter H Help");
  if (!browse_valid) {
    for (r = 0; r < LIST_ROWS; r++)
      scr_fill((uint8_t) (LIST_ROW + r), 0, SCR_COLS, ' ');
    scr_text(LIST_ROW + 1, 3, "Pick a host on the Hosts tab", 36);
    return;
  }
  if (list_stale)               /* the page buffer was borrowed: rebuild from what was sent */
    (void) browse_goto(bpage, brow, 0);
  if (core_compose(uri_buf, sizeof(uri_buf), host_ptr[browse_host], path, ""))
    tail_text(ROW_FIELD, uri_buf);
  for (r = 0; r < LIST_ROWS; r++)
    browse_row(r);
  browse_info();
  browse_detail();
}
#pragma code-name(pop)

/* ---- Drives --------------------------------------------------------------------------- */

#pragma code-name(push, "TAB_DRIVES")
static void assign_drive(uint8_t unit, const char *uri, uint8_t readonly)
{
  if (!core_put_slot(unit, uri, readonly)) {
    say("Unable to save drive slot", 1);
    return;
  }
  core_drive_tail(unit, uri, (uint16_t) strlen(uri));
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
#pragma code-name(pop)

#pragma code-name(push, "TAB_DRIVES")
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
#pragma code-name(pop)

/* ---- Setup ----------------------------------------------------------------------------- */

#pragma code-name(push, "TAB_SETUP")
SETUP_SCOPE void SETUP_IMPL(setup_refresh)(void)
{
  list_stale = 1;
  memset(&wifi, 0, sizeof(wifi));
  memset(&wifi_cfg, 0, sizeof(wifi_cfg));
  memset(&wifi_info, 0, sizeof(wifi_info));
  memset(&fuji_info, 0, sizeof(fuji_info));
  (void) fn_wifi_get_status(&wifi);
  (void) fn_wifi_get_config(&wifi_cfg);
  (void) fn_wifi_get_adapter_info(&wifi_info);
  (void) fn_fuji_get_info(&fuji_info);
}
#pragma code-name(pop)

#pragma code-name(push, "TAB_SETUP")
static void setup_row(uint8_t n, const char *label, const char *value)
{
  uint8_t row;

  row = (uint8_t) (LIST_ROW + n);
  scr_text(row, 1, label, 12);
  scr_text(row, 14, value, 24);
}
#pragma code-name(pop)

#pragma code-name(push, "TAB_SETUP")
static const char *link_text(void)
{
  switch (wifi.link_state) {
  case 0: return "Disconnected";
  case 1: return "Connecting";
  case 2: return "Connected";
  default: return "Failed";
  }
}

#pragma code-name(pop)

#pragma code-name(push, "TAB_SETUP")
SETUP_SCOPE void SETUP_IMPL(draw_setup)(void)
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
#pragma code-name(pop)

/* Scan for networks and let the user pick one; 0 if none was chosen. */
#define SCAN_MAX 6

#pragma code-name(push, "TAB_SETUP")
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
  scr_present();
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
#pragma code-name(pop)

#pragma code-name(push, "TAB_SETUP")
static void join_wifi(void)
{
  char *ssid;
  char *pass;
  fn_wifi_config_update_t up;
  uint8_t picked;

  /* The page buffer holds the text being edited (and the scan list). */
  ssid = (char *) core_scratch() + 520;     /* past the scan records and their wire buffer */
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
}
#pragma code-name(pop)

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
  for (u = 0; u < SCR_ROWS; u++) {
    scr_theme(u, T_FIELD);
    scr_fill(u, 0, SCR_COLS, ' ');
  }
  scr_text(11, 2, "Mounting disks...", 36);
  scr_present();
  scr_flat_on();
  if (!core_mount_all(&failed) || failed != 0xFF) {
    scr_flat_off();
    say(core_mount_error(), 1);
    return;
  }
  scr_text(11, 2, "Booting...", 36);
  scr_present();
  scr_coldstart();
}

/* ---- Mounting a file ---------------------------------------------------------------------- */

#pragma code-name(push, "TAB_BROWSE")
static void open_file(const char *name)
{
  uint8_t readonly;
  uint8_t r;
  uint8_t k;

  if (!core_compose(uri_buf, sizeof(uri_buf), host_ptr[browse_host], path, name) ||
      strlen(uri_buf) > CORE_URI_MAX) {
    say("URI is longer than 196 characters", 1);
    return;
  }
  if (target_unit != NO_UNIT) {
    assign_drive(target_unit, uri_buf, 1);
    drive_sel = target_unit;
    target_unit = NO_UNIT;
    tab = TAB_DRIVES;
    return;
  }
  readonly = 1;
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
#pragma code-name(pop)

/* ---- Keys ---------------------------------------------------------------------------------- */

extern uint8_t view_key;
#pragma zpsym("view_key")
uint8_t view_move(void);
#define move_cursor(k, p, t) (view_key = (k), view_pos = (p), view_total = (t), view_move())

/* Packed help text avoids a large cc65 call sequence on this tight target.
 * | separates display rows and each tab's text ends with a NUL. */
static const char help_hosts[] = "Hosts|RET browse, A add, E edit, D delete";
static const char help_browse[] = "Browse|RET open; up/down move; </> page|SPACE/B page; BS up; R reload|F filter (* ?; blank clears)";
static const char help_drives[] = "Drives|RET pick; X eject; R protect|1-8 select drive";
static const char help_setup[] = "Setup|RET join Wi-Fi; R refresh";
static const char *const help_tab[] = { help_hosts, help_browse, help_drives, help_setup };
static const char help_global[] = "TAB/SELECT change tab|OPTION/START boot|Joystick moves; trigger RET|H or HELP opens this";

void help_rows_run(void);
#define help_rows(row, p) do { scr_zr = (row); scr_zs = (p); help_rows_run(); } while (0)

/* Called only by the main loop: editors and existing dialogs consume H themselves. */
static void show_help(void)
{
  uint8_t r;
  for (r = 5; r < 19; r++) { scr_theme(r, T_DIALOG); scr_fill(r, 0, SCR_COLS, ' '); }
  scr_text(5, 2, "Help - press any key", 36);
  help_rows(7, help_tab[tab]);
  help_rows(13, help_global);
  (void) input_key();
  input_settle();
}

#pragma code-name(push, "TAB_HOSTS")
static void key_hosts(uint8_t k)
{
  uint16_t pos;

  pos = host_sel;
  if (move_cursor(k, &pos, host_count)) {
    host_sel = (uint8_t) pos;
    keep_in_view(&pos, &top, host_count);   /* scroll if the bar left the page */
    cursor_only = 1;
    return;
  }
  if (k == K_RET && host_count) {
    browse_enter_host(host_sel);
    tab = TAB_BROWSE;
  } else if (k == 'a' && host_count < CORE_MAX_HOSTS) {
    uri_buf[0] = 0;
    if (edit_line("New host or URI", "RET Add ESC Cancel", uri_buf,
                  CORE_HOST_MAX + 1, 0) && uri_buf[0]) {
      if (!core_host_set(host_count, uri_buf)) {
        say("No room for another host", 1);
      } else {
        host_sel = (uint8_t) (host_count - 1);
        say(core_save_hosts() ? "Host added" : "Unable to save hosts", 0);
      }
    }
  } else if (k == 'e' && host_count) {
    strcpy(uri_buf, host_ptr[host_sel]);
    if (edit_line("Edit host or URI", "RET Save ESC Cancel", uri_buf,
                  CORE_HOST_MAX + 1, 0) && uri_buf[0]) {
      if (!core_host_set(host_sel, uri_buf))
        say("No room for a longer host", 1);
      else
        say(core_save_hosts() ? "Host saved" : "Unable to save hosts", 0);
    }
  } else if (k == 'd' && host_count) {
    if (confirm("Delete this host? Y/N")) {
      core_host_delete(host_sel);
      if (host_sel >= host_count && host_sel)
        host_sel--;
      say(core_save_hosts() ? "Host deleted" : "Unable to save hosts", 0);
    }
  }
}
#pragma code-name(pop)

/* Page forward or back, keeping the cursor on the same row where it can. */
#pragma code-name(push, "TAB_BROWSE")
static void browse_page(uint16_t sp, uint8_t row)
{
  (void) browse_goto(sp, row, 0);
}
#pragma code-name(pop)

#pragma code-name(push, "TAB_BROWSE")
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
  if (k == K_UP || k == '-') {
    if (brow) {
      brow--;
      cursor_only = 1;
    } else if (bpage) {                         /* off the top: the page before */
      browse_page((uint16_t) (bpage - 1), LIST_ROWS - 1);
    }
    return;
  }
  if (k == K_DOWN || k == '=') {
    if ((uint8_t) (brow + 1) < entry_count) {
      brow++;
      cursor_only = 1;
    } else if ((uint16_t) (bpage + 1) < npages) { /* off the bottom: the next page */
      browse_page((uint16_t) (bpage + 1), 0);
    }
    return;
  }
  if (k == K_RIGHT || k == ' ' || k == '>') {
    if ((uint16_t) (bpage + 1) < npages)
      browse_page((uint16_t) (bpage + 1), brow);
    return;
  }
  if (k == K_LEFT || k == 'b' || k == '<') {
    if (bpage)
      browse_page((uint16_t) (bpage - 1), brow);
    else
      k = K_BS;                                 /* already on the first page: up a level */
    if (k != K_BS)
      return;
  }
  if (k == K_BS || k == 'u') {
    if (path[0]) {
      parent_path();
      browse_reset();
    }
    return;
  }
  if (k == 'r') {                               /* have the device read the folder again */
    core_dir_reset();
    (void) browse_goto(bpage, brow, 1);
    return;
  }
  if (k == 'f' || k == '/') {
    strcpy(uri_buf, filter);
    if (edit_line("Name filter", "RET Apply ESC Cancel; blank clears", uri_buf,
                  sizeof(filter), 0)) {
      strcpy(filter, uri_buf);
      browse_reset();
      say(filter[0] ? "Filter applied" : "Filter cleared", 0);
    }
    return;
  }
  if (k != K_RET || brow >= entry_count)
    return;
  e = &entries[brow];
  /* A page keeps only the start of a long name: ask for the whole one. */
  isdir = (uint8_t) (e->flags & CORE_ENTRY_DIR);
  fullname = (char *) core_scratch();   /* the page buffer: rebuilt afterwards */
  if (e->flags & CORE_ENTRY_TRUNCATED) {
    if (!core_compose(uri_buf, sizeof(uri_buf), host_ptr[browse_host], path, "") ||
        !core_full_name(uri_buf, filter, (uint16_t) (bpage * LIST_ROWS + brow),
                        fullname, CORE_URI_MAX + 1)) {
      say("Name is longer than 196 characters", 1);
      return;
    }
  } else {
    memmove(fullname, e->name, strlen(e->name) + 1);
  }
  if (isdir) {
    len = (uint8_t) strlen(path);
    nlen = (uint8_t) strlen(fullname);
    if ((uint16_t) (len + nlen + 2) > CORE_URI_MAX) {
      say("Path is longer than 196 characters", 1);
      return;
    }
    strcpy(path + len, fullname);
    path[len + nlen] = '/';
    path[len + nlen + 1] = 0;
    browse_reset();
  } else {
    open_file(fullname);
  }
}
#pragma code-name(pop)

#pragma code-name(push, "TAB_DRIVES")
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
#pragma code-name(pop)

#pragma code-name(push, "TAB_SETUP")
SETUP_SCOPE void SETUP_IMPL(key_setup)(uint8_t k)
{
  if (k == 'r') {
    SETUP_IMPL(setup_refresh)();
    say("Refreshed", 0);
  } else if (k == K_RET) {
    join_wifi();
    SETUP_IMPL(setup_refresh)();
  }
}
#pragma code-name(pop)

/* The cursor moved but the page did not scroll: only the selection bar, the
 * field and the info line change, so redraw just those. */
static void update_selection(void)
{
  uint16_t at;

  /* move the bar: the old row back to body colour, the new row orange */
  if (sel_row != 0xFF)
    scr_theme(sel_row, T_BODY);
  at = tab == TAB_HOSTS ? host_sel : tab == TAB_BROWSE ? brow : drive_sel;
  list_row((uint8_t) (LIST_ROW + at - (tab == TAB_BROWSE ? 0 : top)), 1);
  if (tab == TAB_HOSTS) {
    scr_fill(ROW_FIELD, 0, SCR_COLS, ' ');
    scr_text(ROW_FIELD, 1, host_count ? host_ptr[host_sel] : "", 38);
  } else if (tab == TAB_DRIVES) {
    scr_fill(ROW_FIELD, 0, SCR_COLS, ' ');
    if (drive_on[drive_sel])
      tail_text(ROW_FIELD, drive_uri[drive_sel]);
  } else {                      /* the path above the list has not changed */
    browse_info();
    browse_detail();
  }
}

/* The Hosts list moved by one row: slide the rows already on screen and draw
 * only the one that came into view. */
static void scroll_one(uint16_t old_top)
{
  uint8_t r;
  uint8_t *dst;
  uint8_t *src;

  if (top > old_top) {                  /* rows move up */
    for (r = LIST_ROW; r < LIST_ROW + LIST_ROWS - 1; r++) {
      dst = (uint8_t *) (scr_rowlo[r] | (scr_rowhi[r] << 8));
      src = (uint8_t *) (scr_rowlo[r + 1] | (scr_rowhi[r + 1] << 8));
      memcpy(dst, src, SCR_COLS);
    }
    r = LIST_ROWS - 1;
  } else {                              /* rows move down */
    for (r = LIST_ROW + LIST_ROWS - 1; r > LIST_ROW; r--) {
      dst = (uint8_t *) (scr_rowlo[r] | (scr_rowhi[r] << 8));
      src = (uint8_t *) (scr_rowlo[r - 1] | (scr_rowhi[r - 1] << 8));
      memcpy(dst, src, SCR_COLS);
    }
    r = 0;
  }
  hosts_row(r);
  scr_scrollbar(LIST_ROW, LIST_ROWS, top, LIST_ROWS, host_count);
}

static void set_tab(uint8_t t)
{
  tab = t;
  top = 0;
  if (t == TAB_SETUP)
    setup_refresh();
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
#ifdef DRAW_TIMING
      uint8_t t0 = *(volatile uint8_t *) 0x14;
#endif
      switch (tab) {
      case TAB_HOSTS:  draw_hosts();  break;
      case TAB_BROWSE: draw_browse(); break;
      case TAB_DRIVES: draw_drives(); break;
      default:         draw_setup();  break;
      }
#ifdef DRAW_TIMING
      dbg_draw = (uint8_t) (*(volatile uint8_t *) 0x14 - t0);
#endif
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
    if (k == K_HELP || k == 'h' || k == 'H') {
      show_help();
      continue;
    }
    /* In a list, right opens the item and left goes up a level (so the
     * joystick can browse); the tabs change with TAB or SELECT. */
    if (tab != TAB_BROWSE) {            /* Browse uses left/right to change page */
      if (k == K_RIGHT && tab != TAB_SETUP)
        k = K_RET;
      else if (k == K_LEFT || k == K_RIGHT)
        continue;
    }
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
    if (cursor_only && tab == old_tab && list_gen == old_gen &&
        (top == old_top ||
         (tab == TAB_HOSTS && (top == old_top + 1 || top + 1 == old_top)))) {
#ifdef DRAW_TIMING
      {
        uint8_t t1 = *(volatile uint8_t *) 0x14;
#endif
      if (top != old_top)
        scroll_one(old_top);
      /* keep_in_view may still scroll on the next draw; here it did not */
      status[0] = 0;            /* the size/date of the new entry replaces it */
      status_error = 0;
      scr_theme(ROW_STATUS, T_STATUS);
      scr_fill(ROW_STATUS, 0, SCR_COLS, ' ');
      update_selection();
#ifdef DRAW_TIMING
      dbg_move = (uint8_t) (*(volatile uint8_t *) 0x14 - t1);
      }
#endif
      redraw = 0;
    }
  }
}

int main(void)
{
  uint8_t failed;

  path[0] = 0;
  SHFLOK = 0;                   /* lower case by default: URIs are case sensitive */
  scr_init();
  scr_theme(ROW_STATUS, T_STATUS);
  scr_text(ROW_STATUS, 1, "Starting FujiNet...", 38);
  scr_present();

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
    scr_present();
    for (;;)
      ;
  }
  input_settle();
  run();
  return 0;
}
