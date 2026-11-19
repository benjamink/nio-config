/* config_ui.c -- config-nio screens for the TRS-80 Model 4/4P (80x24 text, keys as in the design:
 * docs/superpowers/specs/2026-11-16-trs80-config-nio-design.md). State, store and FujiNet calls come
 * from the portable layer (config_nio_*, fnsvc_*); this file only draws and handles keys. */
#include "config_ui.h"
#include "config_nio.h"
#include "fujinet-nio.h"
#include "trs80_console.h"
#include "trs80_keys.h"
#include "ui_widgets.h"
#include "wifi_trs80.h"

#include <string.h>

#define SECTOR_SIZE_HINT 256            /* LS-DOS images are 256-byte sectors */
#define PAGE CONFIG_NIO_MAX_ENTRIES     /* entries per Browse page; a multiple of FNSVC_PAGE_SIZE (4) */
#define FILES_W 58
#define DRIVES_COL 62
#define ROW_PATH 3
#define ROW_LIST 4
#define ROW_INFO 17
#define ROW_HINT 22
#define ROW_MSG 23

enum { TAB_HOSTS, TAB_BROWSE, TAB_SLOTS, TAB_SETUP, TAB_COUNT };
enum { PANE_FILES, PANE_DRIVES };

static const char *const tab_names[TAB_COUNT] = {"Hosts", "Browse", "Slots", "Setup"};

static config_nio_state_t st;
static uint8_t tab = TAB_BROWSE;
static uint8_t pane = PANE_FILES;
static uint8_t cur_host;
static uint8_t quit;

/* Browse: the current page of the folder is in st.entries; page_base is the absolute index of its first entry. */
static uint16_t page_base;
static uint32_t folder_total;           /* entries in the folder, capped at 65535 (list indexes are 16-bit) */
static uint8_t folder_over;             /* the folder really has more than 65535 entries */
static uint8_t paged;                   /* 0 when the firmware has no ListPages: only the first PAGE entries are reachable */
static uint8_t listed;                  /* a folder has been loaded for cur_host/browse_path */
static list_t files;
static list_t drives;
static list_t hostlist;
static list_t netlist;
static wifi_net_t nets[WIFI_SCAN_MAX];
static uint8_t net_count;
static char wifi_line[40];

static char linebuf[CON_COLS + 1];
static char uri_buf[FNSVC_MAX_URI + 1];
static char name_buf[CONFIG_NIO_URI_MAX + 1];

/* ---------------------------------------------------------------- helpers */

static void put_uint(char *out, uint32_t v, uint8_t *pos)
{
  char tmp[11];
  uint8_t n = 0;

  do {
    tmp[n++] = (char) ('0' + v % 10);
    v /= 10;
  } while (v);
  while (n)
    out[(*pos)++] = tmp[--n];
}

static void put_size(char *out, uint32_t size, uint8_t *pos)
{
  if (size >= 1048576UL) {
    put_uint(out, size / 1048576UL, pos);
    out[(*pos)++] = 'M';
  } else if (size >= 1024) {
    put_uint(out, size / 1024, pos);
    out[(*pos)++] = 'K';
  } else {
    put_uint(out, size, pos);
    out[(*pos)++] = 'B';
  }
}

static const char *base_name(const char *uri)
{
  const char *p = uri, *last = uri;

  while (*p) {
    if (*p == '/' || *p == '\\' || *p == ':')
      last = p + 1;
    p++;
  }
  return last;
}

/* Set by a key handler that changed more than it could repaint itself; the main loop then runs draw_all().
 * Handlers repaint just the rows they change (a full repaint is thousands of console calls). */
static uint8_t redraw = 1;
static uint8_t msg_up;                  /* a message is on the message row: clear it at the next key */

static void message(const char *text)
{
  msg_draw(ROW_MSG, text);
  msg_up = text[0] != 0;
}

static void say_status(void)
{
  message(st.status);
}

/* ---------------------------------------------------------------- drives */

static uint8_t drive_mounted(uint8_t unit)
{
  return st.slots[unit].enabled && st.slots[unit].uri[0];
}

/* Drive :0 holds the boot disk (LS-DOS, the F* commands and CONFIG itself). Replacing it from inside
 * CONFIG would strand the user without them until a full restart, so CONFIG leaves it alone; FMOUNT
 * can still change it deliberately. */
static uint8_t boot_drive_guard(uint8_t unit)
{
  if (unit != 0)
    return 0;
  message("Drive :0 is the boot disk; use FMOUNT :0 to change it");
  return 1;
}

static const char *drive_item(uint16_t unit, void *ctx)
{
  uint8_t n = 0;
  const char *name;

  (void) ctx;
  linebuf[n++] = ':';
  linebuf[n++] = (char) ('0' + unit);
  linebuf[n++] = ' ';
  if (unit == 0) {
    const char *b = "(boot disk)";
    while (*b)
      linebuf[n++] = *b++;
  } else if (drive_mounted((uint8_t) unit)) {
    name = base_name(st.slots[unit].uri);
    while (*name && n < 14)
      linebuf[n++] = *name++;
    while (n < 15)
      linebuf[n++] = ' ';
    linebuf[n++] = strcmp(st.slots[unit].mode, "r") == 0 ? 'R' : 'W';
    linebuf[n++] = strcmp(st.slots[unit].mode, "r") == 0 ? 'O' : ' ';
  } else {
    const char *e = "(empty)";
    while (*e)
      linebuf[n++] = *e++;
  }
  linebuf[n] = 0;
  return linebuf;
}

static void drives_init(void)
{
  drives.row = ROW_LIST;
  drives.col = DRIVES_COL;
  drives.width = 17;
  drives.height = FNCTL_MAX_UNITS;
  drives.count = FNCTL_MAX_UNITS;
  drives.item = drive_item;
  drives.ctx = 0;
  drives.sel = 2;                          /* :0 is the boot disk; :1 is left for a physical floppy */
}

static int mount_failure_text(void)
{
  switch (fnsvc_disk_last_error()) {
  case FN_DISK_ERR_GEOMETRY_REQUIRED: message("Mount: sector size needed"); break;
  case FN_DISK_ERR_INVALID_GEOMETRY:  message("Mount: wrong sector size"); break;
  case FN_DISK_ERR_FILE_NOT_FOUND:    message("Mount: image not found"); break;
  case FN_DISK_ERR_BAD_IMAGE:         message("Mount: bad image"); break;
  case FN_DISK_ERR_UNSUPPORTED_TYPE:  message("Mount: unsupported type"); break;
  default:                            message("Mount failed"); break;
  }
  return 0;
}

/* Put `uri` on LS-DOS drive :unit: remember it in the catalogue slot and the drive map, then mount it. */
/* Drives whose LS-DOS disk driver is not installed yet (bit n = drive :n). The driver cannot be installed from
 * inside CONFIG (it lowers HIGH$ over memory CONFIG's client occupies), so quitting runs FUJIDRV for them. */
static uint8_t driver_wanted;

static int mount_drive(uint8_t unit, const char *uri, uint8_t readonly)
{
  config_nio_mapping_t m;

  if (boot_drive_guard(unit))
    return 0;
  if (!fnsvc_disk_mount(unit, uri, readonly, SECTOR_SIZE_HINT))
    return mount_failure_text();
  if (!config_nio_write_slot(&st, unit, uri, readonly ? "r" : "rw")) {
    message("Mounted, but the slot could not be saved");
    return 0;
  }
  m.valid = 1;
  m.slot = unit;
  m.readonly = readonly;
  config_nio_mapping_set(&st, unit, &m);
  if (!config_nio_save_mappings(&st)) {
    message("Mounted, but the drive map could not be saved");
    return 0;
  }
  if (unit > 0 && !dos_drive_enabled(unit))
    driver_wanted |= (uint8_t) (1 << unit);
  return 1;
}

static void eject_drive(uint8_t unit)
{
  if (boot_drive_guard(unit))
    return;
  if (!drive_mounted(unit)) {
    message("Nothing to eject");
    return;
  }
  if (!fnsvc_disk_unmount(unit)) {
    message("Eject failed");
    return;
  }
  config_nio_mapping_clear(&st, unit);
  if (!config_nio_save_mappings(&st) || !config_nio_delete_slot(&st, unit)) {
    message("Ejected, but the saved slot could not be cleared");
    return;
  }
  message("Ejected");
}

/* Replace what is mounted on `unit` with `uri`. `uri` must not point into st.slots (the slot is rewritten).
 * If the new mount fails, put the previous image back (the saved slot still names it), so a failed attempt
 * (restored from the saved slot, which a failed mount leaves untouched) does not leave the drive unmounted on the FujiNet while the saved slot says it is mounted. If even that
 * fails, clear the saved state so it matches the device. Returns 1 when `uri` is mounted. */
static int remount_drive(uint8_t unit, const char *uri, uint8_t readonly)
{
  uint8_t old_ro = strcmp(st.slots[unit].mode, "r") == 0;

  fnsvc_disk_unmount(unit);
  if (mount_drive(unit, uri, readonly))
    return 1;
  if (!fnsvc_disk_mount(unit, st.slots[unit].uri, old_ro, SECTOR_SIZE_HINT)) {
    config_nio_mapping_clear(&st, unit);
    config_nio_save_mappings(&st);
    config_nio_delete_slot(&st, unit);
    message("Mount failed; drive is now empty");
  }
  return 0;
}

static void toggle_readonly(uint8_t unit)
{
  uint8_t ro;

  if (boot_drive_guard(unit))
    return;
  if (!drive_mounted(unit)) {
    message("Drive is empty");
    return;
  }
  ro = strcmp(st.slots[unit].mode, "r") != 0;
  strcpy(name_buf, st.slots[unit].uri);      /* static buffer: keep the 129-byte copy off the small stack */
  if (remount_drive(unit, name_buf, ro))
    message(ro ? "Now read-only" : "Now read-write");
}

/* ---------------------------------------------------------------- browse */

static void collect(uint8_t is_dir, const char *name, uint32_t size, uint32_t mtime, void *ctx)
{
  config_nio_entry_t *e;
  uint16_t n;

  (void) ctx;
  if (st.entry_count >= CONFIG_NIO_MAX_ENTRIES) {
    st.entries_truncated = 1;
    return;
  }
  e = &st.entries[st.entry_count++];
  e->is_dir = is_dir;
  e->size = size;
  e->mtime = mtime;
  n = (uint16_t) strlen(name);
  if (n > CONFIG_NIO_NAME_MAX)
    n = CONFIG_NIO_NAME_MAX;
  memcpy(e->name, name, n);
  e->name[n] = 0;
}

/* "Browse failed: error E status S" (fnsvc_last_error / fnsvc_last_status), as MS-DOS reports it. */
static void browse_failed(void)
{
  uint8_t n = 0;
  const char *t = "Browse failed: error ";

  while (*t)
    linebuf[n++] = *t++;
  put_uint(linebuf, fnsvc_last_error(), &n);
  t = " status ";
  while (*t)
    linebuf[n++] = *t++;
  put_uint(linebuf, fnsvc_last_status(), &n);
  linebuf[n] = 0;
  message(linebuf);
}

static int folder_uri(void)
{
  if (!st.host_count) {
    message("No hosts: add one on the Hosts tab (H)");
    return 0;
  }
  if (!config_nio_compose_uri(st.hosts[cur_host], st.browse_path, "", uri_buf, sizeof(uri_buf))) {
    message("Path is too long");
    return 0;
  }
  return 1;
}

static const char *file_item(uint16_t idx, void *ctx)
{
  const config_nio_entry_t *e;
  uint8_t n = 0, len, pad;

  (void) ctx;
  if (idx < page_base || idx >= page_base + st.entry_count) {
    linebuf[0] = 0;
    return linebuf;
  }
  e = &st.entries[idx - page_base];
  len = (uint8_t) strlen(e->name);
  if (len > 40)
    len = 40;
  memcpy(linebuf, e->name, len);
  n = len;
  if (e->is_dir & CONFIG_NIO_ENTRY_FLAG_DIR)
    linebuf[n++] = '/';
  pad = (e->is_dir & CONFIG_NIO_ENTRY_FLAG_DIR) ? 0 : 1;
  while (n < 44)
    linebuf[n++] = ' ';
  if (pad)
    put_size(linebuf, e->size, &n);
  linebuf[n] = 0;
  return linebuf;
}

static void files_init(void)
{
  files.row = ROW_LIST;
  files.col = 1;
  files.width = FILES_W;
  files.height = PAGE;
  files.count = 0;
  files.sel = 0;
  files.top = 0;
  files.item = file_item;
  files.ctx = 0;
}

/* Load the page that holds absolute index `at` (a multiple of PAGE). Returns 0 on failure, with a message. */
static int load_page(uint16_t at, uint8_t refresh)
{
  uint16_t delivered;
  uint32_t total = 0;

  if (!folder_uri())
    return 0;
  st.entry_count = 0;
  st.entries_truncated = 0;
  page_base = at;
  if (paged) {
    do {
      delivered = 0;
      if (!fnsvc_list_pages(uri_buf, (uint32_t) (uint16_t) (at + st.entry_count), refresh, collect, 0, &total, &delivered))
        break;
      refresh = 0;
    } while (delivered && st.entry_count < PAGE && (uint32_t) (uint16_t) (at + st.entry_count) < total);
    if (fnsvc_last_error() == FNSVC_ERR_NONE) {
      folder_over = total > 0xFFFFUL;
      folder_total = folder_over ? 0xFFFFUL : total;
      files.count = (uint16_t) folder_total;
      return 1;
    }
    if (fnsvc_last_error() != FNSVC_ERR_UNSUPPORTED) {
      browse_failed();
      return 0;
    }
    paged = 0;                           /* firmware without ListPages: fall back to the first PAGE entries */
    st.entry_count = 0;
  }
  page_base = 0;
  if (!fnsvc_list_directory_flags(uri_buf, collect, 0)) {
    browse_failed();
    return 0;
  }
  folder_over = 0;
  folder_total = st.entry_count;
  files.count = st.entry_count;
  if (st.entries_truncated)
    message("Only the first entries are shown (this firmware cannot page)");
  return 1;
}

static void open_folder(uint8_t refresh)
{
  paged = 1;
  files.sel = 0;
  files.top = 0;
  listed = load_page(0, refresh);
  if (!listed) {
    st.entry_count = 0;
    files.count = 0;
    page_base = 0;
  }
}

static void path_up(void)
{
  uint16_t len = (uint16_t) strlen(st.browse_path);

  while (len > 0 && st.browse_path[len - 1] == '/')
    st.browse_path[--len] = 0;
  while (len > 0 && st.browse_path[len - 1] != '/')
    st.browse_path[--len] = 0;
}

static void enter_folder(const char *name)
{
  uint16_t len = (uint16_t) strlen(st.browse_path);
  uint16_t nlen = (uint16_t) strlen(name);

  if ((uint16_t) (len + nlen + 2) > CONFIG_NIO_PATH_MAX) {
    message("Path is too long");
    return;
  }
  memcpy(&st.browse_path[len], name, nlen);
  st.browse_path[len + nlen] = '/';
  st.browse_path[len + nlen + 1] = 0;
}

static void draw_counter(void);

static void draw_browse(void)
{
  uint8_t n;

  con_fill(ROW_PATH, 0, CON_COLS, ' ');
  n = 0;
  if (st.host_count) {
    const char *h = st.hosts[cur_host];
    while (*h && n < FILES_W - 2)
      linebuf[n++] = *h++;
    h = st.browse_path;
    if (n && linebuf[n - 1] != '/' && *h)
      linebuf[n++] = '/';
    while (*h && n < FILES_W)
      linebuf[n++] = *h++;
  }
  linebuf[n] = 0;
  con_put(ROW_PATH, 1, linebuf);
  con_put(ROW_PATH, DRIVES_COL, "DRIVES");
  for (n = ROW_PATH; n < ROW_INFO; n++)
    con_put(n, 60, "|");
  if (listed)
    list_draw(&files);
  else
    for (n = ROW_LIST; n < ROW_INFO; n++)
      con_fill(n, 1, FILES_W, ' ');
  list_draw(&drives);
  con_fill(ROW_INFO, 0, CON_COLS, '-');
  draw_counter();
}

static void draw_counter(void)
{
  uint8_t n = 0;

  if (listed && folder_total) {
    put_uint(linebuf, (uint32_t) (uint16_t) (files.sel + 1), &n); /* 16-bit add; see "z88dk pitfalls" in docs/trs80-config.md */
    linebuf[n++] = '/';
    put_uint(linebuf, folder_total, &n);
    if (folder_over)                       /* only the first 65535 entries are listed */
      linebuf[n++] = '+';
  }
  linebuf[n] = 0;
  con_fill(ROW_INFO + 1, 0, CON_COLS, ' ');
  con_put(ROW_INFO + 1, 1, linebuf);
  con_put(ROW_INFO + 1, DRIVES_COL, pane == PANE_DRIVES ? "[drives]" : " drives ");
  con_put(ROW_INFO + 1, 40, pane == PANE_FILES ? "[files]" : " files ");
}

static void files_move(int key)
{
  uint16_t old_sel = files.sel;
  int r = list_key(&files, key);
  uint16_t want;

  if (r != LIST_MOVED)
    return;
  want = (uint16_t) ((files.sel / PAGE) * PAGE);
  if (want != page_base && paged) {
    redraw = 1;                          /* a new page: repaint everything */
    if (!load_page(want, 0))
      return;
    files.top = page_base;
    return;
  }
  files.top = page_base;
  if (old_sel >= files.top && old_sel < files.top + files.height) {
    list_draw_row(&files, (uint8_t) (old_sel - files.top));
    list_draw_row(&files, (uint8_t) (files.sel - files.top));
    draw_counter();
  } else
    redraw = 1;
}

static void drive_row_changed(void);

static void files_enter(void)
{
  const config_nio_entry_t *e;

  if (!listed || files.sel < page_base || files.sel >= page_base + st.entry_count)
    return;
  e = &st.entries[files.sel - page_base];
  if (e->is_dir & CONFIG_NIO_ENTRY_FLAG_NAME_TRUNCATED) {
    message("Name too long for this client");
    return;
  }
  if (e->is_dir & CONFIG_NIO_ENTRY_FLAG_DIR) {
    enter_folder(e->name);
    open_folder(0);
    redraw = 1;
    return;
  }
  if (!config_nio_compose_uri(st.hosts[cur_host], st.browse_path, e->name, name_buf, sizeof(name_buf))) {
    message("URI is too long");
    return;
  }
  if (mount_drive((uint8_t) drives.sel, name_buf, 0)) {
    strcpy(linebuf, "Mounted ");
    strncat(linebuf, e->name, 24);
    strcat(linebuf, " on :");
    linebuf[strlen(linebuf) + 1] = 0;
    linebuf[strlen(linebuf)] = (char) ('0' + drives.sel);
    if (driver_wanted & (1 << drives.sel))     /* :0 has the driver from boot */
      strcat(linebuf, "  (FUJIDRV runs when you quit)");
    message(linebuf);
  }
  drive_row_changed();
}

/* Redraw only the selected drive's row (after an eject, read-only toggle or mount), not the whole screen. */
static void drive_row_changed(void)
{
  list_draw_row(&drives, (uint8_t) (drives.sel - drives.top));
}

static void browse_key(int key)
{
  if (key == KEY_TAB) {
    pane = pane == PANE_FILES ? PANE_DRIVES : PANE_FILES;
    draw_counter();                      /* only the [drives]/[files] markers change */
    return;
  }
  if (pane == PANE_DRIVES) {
    list_key_draw(&drives, key);
    if (key == KEY_ENTER) {
      pane = PANE_FILES;
      draw_counter();
    } else if (key == 'x' || key == 'X') {
      eject_drive((uint8_t) drives.sel);
      drive_row_changed();
    } else if (key == 'r' || key == 'R') {
      toggle_readonly((uint8_t) drives.sel);
      drive_row_changed();
    }
    return;
  }
  if (key == KEY_ENTER)
    files_enter();
  else if (key == KEY_BKSP) {            /* Left arrow: up one folder */
    if (st.browse_path[0]) {
      path_up();
      open_folder(0);
      redraw = 1;
    }
  } else if (key == 'r' || key == 'R') {
    open_folder(1);
    redraw = 1;
  } else
    files_move(key);
}

/* ---------------------------------------------------------------- slots tab */

/* One slot's two rows: ":2 <image> RW" and its URI. Used for a single slot when only it changed.
 * drive_item() returns linebuf; con_put() copies it out before linebuf is reused for the URI. */
static void draw_slot(uint8_t u)
{
  uint8_t n;

  con_fill(ROW_LIST + u * 2, 0, CON_COLS, ' ');
  con_put(ROW_LIST + u * 2, 0, u == drives.sel ? ">" : " ");
  con_put(ROW_LIST + u * 2, 2, drive_item(u, 0));
  con_fill(ROW_LIST + u * 2 + 1, 0, CON_COLS, ' ');
  if (drive_mounted(u)) {
    n = 0;
    while (st.slots[u].uri[n] && n < 70) {
      linebuf[n] = st.slots[u].uri[n];
      n++;
    }
    linebuf[n] = 0;
    con_put(ROW_LIST + u * 2 + 1, 6, linebuf);
  }
}

static void draw_slots(void)
{
  uint8_t u;

  for (u = 0; u < FNCTL_MAX_UNITS; u++)
    draw_slot(u);
}

static void slots_key(int key)
{
  uint8_t old = (uint8_t) drives.sel;

  if (key == KEY_UP && drives.sel > 0)
    drives.sel--;
  else if (key == KEY_DOWN && drives.sel + 1 < FNCTL_MAX_UNITS)
    drives.sel++;
  else if (key == 'x' || key == 'X')
    eject_drive(old);
  else if (key == 'r' || key == 'R')
    toggle_readonly(old);
  else if (key == 'e' || key == 'E' || key == KEY_ENTER) {
    name_buf[0] = 0;
    if (boot_drive_guard(old))
      return;
    if (drive_mounted(old))
      strcpy(name_buf, st.slots[old].uri);
    message("Image URI (ENTER mounts, BREAK cancels):");
    if (field_edit(name_buf, CONFIG_NIO_URI_MAX, ROW_MSG, 1, 76) && name_buf[0]) {
      /* keep an existing read-only setting; an empty drive mounts read-write */
      uint8_t ro = drive_mounted(old) && strcmp(st.slots[old].mode, "r") == 0;

      if (remount_drive(old, name_buf, ro))
        message("Mounted");
    }
  } else
    return;
  /* Only the slot that was acted on and the one now selected change: redraw those rows, not the screen. */
  draw_slot(old);
  draw_slot((uint8_t) drives.sel);
}

/* ---------------------------------------------------------------- hosts tab */

static const char *host_item(uint16_t i, void *ctx)
{
  (void) ctx;
  strncpy(linebuf, st.hosts[i], 72);
  linebuf[72] = 0;
  return linebuf;
}

static void hosts_init(void)
{
  hostlist.row = ROW_LIST;
  hostlist.col = 1;
  hostlist.width = 76;
  hostlist.height = CONFIG_NIO_MAX_HOSTS;
  hostlist.count = st.host_count;
  hostlist.sel = 0;
  hostlist.top = 0;
  hostlist.item = host_item;
  hostlist.ctx = 0;
}

static void hosts_save(void)
{
  hostlist.count = st.host_count;
  if (hostlist.sel >= st.host_count && st.host_count)
    hostlist.sel = st.host_count - 1;
  if (!config_nio_save_hosts(&st))
    message("Could not save the host list");
}

static void host_edit(uint8_t index, uint8_t is_new)
{
  message(is_new ? "New host URI (ENTER saves, BREAK cancels):" : "Host URI (ENTER saves, BREAK cancels):");
  con_fill(ROW_INFO + 2, 0, CON_COLS, ' ');
  strcpy(name_buf, is_new ? "" : st.hosts[index]);
  if (!field_edit(name_buf, CONFIG_NIO_URI_MAX, ROW_INFO + 2, 1, 76) || !name_buf[0]) {
    message("Cancelled");
    return;
  }
  config_nio_host_set(&st, index, name_buf);
  if (is_new)
    st.host_count++;
  hosts_save();
  message("Saved");
}

static void draw_hosts(void);

/* After an add, edit or delete: repaint the list and wipe the edit field's row, not the screen. */
static void hosts_changed(void)
{
  draw_hosts();
  con_fill(ROW_INFO + 2, 0, CON_COLS, ' ');
}

static void hosts_key(int key)
{
  uint8_t i;

  if (key == 'a' || key == 'A') {
    if (st.host_count >= CONFIG_NIO_MAX_HOSTS)
      message("The host list is full");
    else
      host_edit(st.host_count, 1);
    hosts_changed();
  } else if ((key == 'e' || key == 'E') && st.host_count) {
    host_edit((uint8_t) hostlist.sel, 0);
    hosts_changed();
  } else if ((key == 'd' || key == 'D') && st.host_count) {
    for (i = (uint8_t) hostlist.sel; (uint8_t) (i + 1) < st.host_count; i++)
      config_nio_host_set(&st, i, st.hosts[i + 1]);
    config_nio_host_clear(&st, (uint8_t) (st.host_count - 1));
    st.host_count--;
    hosts_save();
    message("Deleted");
    hosts_changed();
  } else if (key == KEY_ENTER && st.host_count) {
    cur_host = (uint8_t) hostlist.sel;
    st.browse_path[0] = 0;
    tab = TAB_BROWSE;
    open_folder(0);
    redraw = 1;
  } else
    list_key_draw(&hostlist, key);
}

static void draw_hosts(void)
{
  con_put(ROW_PATH, 1, "HOSTS");
  list_draw(&hostlist);                  /* pads the unused rows with blanks */
  if (!st.host_count)
    con_put(ROW_LIST, 3, "(none yet - press A to add, e.g. tnfs://host or sd0:/)");
}

/* ---------------------------------------------------------------- setup tab */

static const char *net_item(uint16_t i, void *ctx)
{
  uint8_t n = 0;
  const char *p = nets[i].ssid;

  (void) ctx;
  while (*p && n < 34)
    linebuf[n++] = *p++;
  while (n < 36)
    linebuf[n++] = ' ';
  linebuf[n++] = nets[i].auth ? '*' : ' ';
  linebuf[n] = 0;
  return linebuf;
}

static void setup_init(void)
{
  netlist.row = ROW_LIST + 2;
  netlist.col = 1;
  netlist.width = 44;
  netlist.height = WIFI_SCAN_MAX;
  netlist.count = 0;
  netlist.sel = 0;
  netlist.top = 0;
  netlist.item = net_item;
  netlist.ctx = 0;
}

static void wifi_refresh_status(void)
{
  uint8_t link;
  char ip[20];

  strcpy(wifi_line, "Wi-Fi: status unavailable");
  if (wifi_status(&link, ip, sizeof(ip))) {
    strcpy(wifi_line, link == 2 ? "Wi-Fi: connected " : link == 1 ? "Wi-Fi: connecting" : "Wi-Fi: not connected");
    if (link == 2)
      strcat(wifi_line, ip);
  }
}

static void do_join(const char *ssid, const char *password)
{
  uint8_t state = 1, tries = 0;

  message("Joining...");
  if (!wifi_join(ssid, password)) {
    message("The FujiNet refused the join request");
    return;
  }
  while (tries++ < 40 && wifi_join_state(&state) && state == 1)
    ;
  wifi_refresh_status();
  message(state == 2 ? "Connected" : state == 3 ? "Join failed (check the password)" : "Join still pending");
}

static void ask_password_and_join(const char *ssid)
{
  char pw[65];

  message("Password (ENTER joins, BREAK cancels):");
  con_fill(ROW_INFO + 2, 0, CON_COLS, ' ');
  pw[0] = 0;
  if (!field_edit(pw, 64, ROW_INFO + 2, 1, 76)) {
    message("Cancelled");
    return;
  }
  do_join(ssid, pw);
}

static void scan_networks(void)
{
  uint8_t more = 0;

  message("Scanning...");
  net_count = 0;
  if (!wifi_scan(0, nets, &net_count, &more)) {
    message(wifi_scan_supported ? "Scan failed" : "This FujiNet can't scan; press J to join by name");
    net_count = 0;
  } else {
    message(!net_count ? "No networks found" : more ? "First networks only; J joins by name" : "Select a network and press ENTER");
  }
  netlist.count = net_count;
  netlist.sel = 0;
  netlist.top = 0;
}

static void draw_wifi(void)
{
  con_fill(ROW_PATH + 1, 0, CON_COLS, ' ');
  con_put(ROW_PATH + 1, 1, wifi_line);
}

static void draw_nets(void)
{
  list_draw(&netlist);                   /* pads the unused rows with blanks */
  if (!net_count)
    con_put(ROW_LIST + 2, 3, "(press N to scan for networks, J to type one)");
}

static void draw_prefs(void)
{
  con_put(ROW_INFO - 4, 1, "PREFERENCES");
  con_put(ROW_INFO - 3, 1, st.prefs.date_format == CONFIG_NIO_PREF_DATE_YDM ? "Date order: YDM   (D toggles)" : "Date order: YMD   (D toggles)");
  con_put(ROW_INFO - 2, 1, st.prefs.size_format == CONFIG_NIO_PREF_SIZE_COMPACT ? "File sizes: compact (Z toggles)" : "File sizes: full    (Z toggles)");
}

/* After a join attempt: the status line may have changed and the edit field's row holds the typed text. */
static void setup_changed(void)
{
  draw_wifi();
  con_fill(ROW_INFO + 2, 0, CON_COLS, ' ');
}

static void setup_key(int key)
{
  if (key == 'n' || key == 'N') {
    scan_networks();
    draw_nets();
  } else if (key == 'j' || key == 'J') {
    message("Network name (ENTER continues, BREAK cancels):");
    con_fill(ROW_INFO + 2, 0, CON_COLS, ' ');
    name_buf[0] = 0;
    if (field_edit(name_buf, WIFI_SSID_MAX, ROW_INFO + 2, 1, 40) && name_buf[0])
      ask_password_and_join(name_buf);
    else
      message("Cancelled");
    setup_changed();
  } else if (key == KEY_ENTER && net_count) {
    strcpy(name_buf, nets[netlist.sel].ssid);
    ask_password_and_join(name_buf);
    setup_changed();
  } else if (key == 'd' || key == 'D') {
    st.prefs.date_format = st.prefs.date_format == CONFIG_NIO_PREF_DATE_YMD ? CONFIG_NIO_PREF_DATE_YDM : CONFIG_NIO_PREF_DATE_YMD;
    message(config_nio_save_prefs(&st) ? "Saved" : "Could not save the preferences");
    draw_prefs();
  } else if (key == 'z' || key == 'Z') {
    st.prefs.size_format = st.prefs.size_format == CONFIG_NIO_PREF_SIZE_FULL ? CONFIG_NIO_PREF_SIZE_COMPACT : CONFIG_NIO_PREF_SIZE_FULL;
    message(config_nio_save_prefs(&st) ? "Saved" : "Could not save the preferences");
    draw_prefs();
  } else
    list_key_draw(&netlist, key);
}

static void draw_setup(void)
{
  con_put(ROW_PATH, 1, "WI-FI");
  draw_wifi();
  draw_nets();
  draw_prefs();
}

/* ---------------------------------------------------------------- tabs */

static uint8_t drawn_tab = 0xFF;

static void draw_all(void)
{
  uint8_t r;

  con_put(0, 1, "CONFIG NIO v1.0  TRS-80 4P");
  tabs_draw(1, tab_names, TAB_COUNT, tab);
  con_fill(2, 0, CON_COLS, '=');
  /* A new tab starts from a blank body (all columns): rows it does not draw would keep the last tab's text. */
  if (tab != drawn_tab) {
    for (r = ROW_PATH; r < ROW_HINT; r++)
      con_fill(r, 0, CON_COLS, ' ');
    drawn_tab = tab;
  }
  if (tab == TAB_BROWSE) {
    draw_browse();
    con_fill(ROW_HINT, 0, CON_COLS, ' ');
    con_put(ROW_HINT, 1, "ENTER mount/open  LEFT up  R reload  TAB pane  X eject  H B S W tabs  Q quit");
  } else if (tab == TAB_SLOTS) {
    draw_slots();
    con_fill(ROW_HINT, 0, CON_COLS, ' ');
    con_put(ROW_HINT, 1, "E/ENTER edit URI  X eject  R read-only on/off  H B S W tabs  Q quit");
  } else if (tab == TAB_HOSTS) {
    draw_hosts();
    con_fill(ROW_INFO, 0, CON_COLS, '-');
    con_fill(ROW_INFO + 2, 0, CON_COLS, ' ');
    con_fill(ROW_HINT, 0, CON_COLS, ' ');
    con_put(ROW_HINT, 1, "A add  E edit  D delete  ENTER browse  H B S W tabs  Q quit");
  } else {
    draw_setup();
    con_fill(ROW_INFO, 0, CON_COLS, '-');
    con_fill(ROW_INFO + 2, 0, CON_COLS, ' ');
    con_fill(ROW_HINT, 0, CON_COLS, ' ');
    con_put(ROW_HINT, 1, "N scan  J join by name  ENTER join  D date  Z sizes  H B S W tabs  Q quit");
  }
}

static int tab_key(int key)
{
  if (key == KEY_SLEFT && tab > 0)
    tab--;
  else if (key == KEY_SRIGHT && tab + 1 < TAB_COUNT)
    tab++;
  else if (key == 'h' || key == 'H')
    tab = TAB_HOSTS;
  else if (key == 'b' || key == 'B')
    tab = TAB_BROWSE;
  else if (key == 's' || key == 'S')
    tab = TAB_SLOTS;
  else if (key == 'w' || key == 'W')
    tab = TAB_SETUP;
  else
    return 0;
  return 1;
}

int config_ui_run(void)
{
  int key;
  uint8_t old_tab;

  config_nio_set_status(&st, "");
  con_clear();
  message("Loading from the FujiNet...");
  if (!config_nio_load(&st)) {
    uint8_t code;
    const char *step = config_nio_load_error(&code);
    con_clear();
    con_put(10, 2, "Cannot load the configuration from the FujiNet (failed step:");
    con_put(11, 2, step);
    con_put(13, 2, "Check the serial cable and that fujinet-nio is running. Press any key.");
    con_getkey();
    con_clear();
    return 2;
  }
  files_init();
  drives_init();
  hosts_init();
  setup_init();
  wifi_refresh_status();
  open_folder(0);
  message("");
  while (!quit) {
    if (redraw) {
      draw_all();
      redraw = 0;
    }
    key = con_getkey();
    if (msg_up)
      message("");
    old_tab = tab;
    if (key == 'q' || key == 'Q' || (key == KEY_BREAK)) {
      quit = 1;
    } else if (tab_key(key)) {
      redraw = tab != old_tab;
      if (tab == TAB_BROWSE && !listed)
        open_folder(0);
    } else if (tab == TAB_BROWSE) {
      browse_key(key);
    } else if (tab == TAB_SLOTS) {
      slots_key(key);
    } else if (tab == TAB_HOSTS) {
      hosts_key(key);
    } else if (tab == TAB_SETUP) {
      setup_key(key);
    }
  }
  con_clear();
  {
    static char cmd[32];
    uint8_t d, n = 0;

    strcpy(cmd, "FUJIDRV");
    n = 7;
    for (d = 1; d < FNCTL_MAX_UNITS; d++) {
      if ((driver_wanted & (1 << d)) && drive_mounted(d)) {
        cmd[n++] = ' ';
        cmd[n++] = ':';
        cmd[n++] = (char) ('0' + d);
      }
    }
    if (n > 7) {
      cmd[n++] = '\r';
      cmd[n] = 0;
      dos_exec(cmd);                       /* installs the drivers, then LS-DOS Ready; does not return */
    }
  }
  return 0;
}
