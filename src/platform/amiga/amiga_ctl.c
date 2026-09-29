#include "amiga_ctl.h"
#include "amiga_fmt.h"
#include "amiga_drives.h"
#include "amiga_help.h"
#include "fujinet-nio.h"

static uint8_t cat_io_buf[1024];
static fn_slot_catalog_io_t cat_io = { cat_io_buf, sizeof(cat_io_buf) };

#include <stdio.h>
#include <string.h>

static void status(amiga_ctl_t *ctl, const char *msg)
{
  config_nio_set_status(ctl->state, msg);
}

static void cat_invalidate(amiga_ctl_t *ctl)
{
  ctl->cat_base[0] = AMIGA_CAT_SLOTS;
  ctl->cat_base[1] = AMIGA_CAT_SLOTS;
  ctl->drive_cached = 0;
}

void amiga_ctl_init(amiga_ctl_t *ctl, config_nio_state_t *state,
                    uint8_t kick13, uint8_t rows, amiga_exec_fn exec,
                    void *exec_ctx)
{
  memset(ctl, 0, sizeof(*ctl));
  ctl->state = state;
  ctl->kick13 = kick13;
  ctl->exec = exec;
  ctl->exec_ctx = exec_ctx;
  ctl->fmount = AMIGA_DEFAULT_FMOUNT;
  ctl->fumount = AMIGA_DEFAULT_FUMOUNT;
  cat_invalidate(ctl);
  amiga_list_init(&ctl->hosts, rows);
  amiga_list_init(&ctl->entries, rows);
  amiga_list_init(&ctl->catalogue, rows);
  amiga_list_init(&ctl->drives, rows);
  amiga_list_init(&ctl->help, rows);
  amiga_list_set_count(&ctl->hosts, state->host_count);
  amiga_list_set_count(&ctl->drives, AMIGA_DRIVE_COUNT);
}

void amiga_ctl_set_tools(amiga_ctl_t *ctl, const char *fmount,
                         const char *fumount)
{
  ctl->fmount = fmount;
  ctl->fumount = fumount;
}

void amiga_ctl_set_rows(amiga_ctl_t *ctl, uint8_t rows)
{
  amiga_list_set_rows(&ctl->hosts, rows);
  amiga_list_set_rows(&ctl->entries, rows);
  amiga_list_set_rows(&ctl->catalogue, rows);
  amiga_list_set_rows(&ctl->drives, rows);
  amiga_list_set_rows(&ctl->help, rows);
}

void amiga_ctl_set_page(amiga_ctl_t *ctl, uint8_t page)
{
  if (page < AMIGA_PAGE_COUNT && page != AMIGA_PAGE_MOUNT &&
      page != AMIGA_PAGE_HELP)
    ctl->page = page;
}

static int selected_host(amiga_ctl_t *ctl, uint8_t *index)
{
  if (ctl->hosts.selected == AMIGA_LIST_NONE ||
      ctl->hosts.selected >= ctl->state->host_count) {
    status(ctl, "No host selected");
    return 0;
  }
  *index = (uint8_t) ctl->hosts.selected;
  return 1;
}

static int save_hosts(amiga_ctl_t *ctl, const char *ok)
{
  amiga_list_set_count(&ctl->hosts, ctl->state->host_count);
  if (!config_nio_save_hosts(ctl->state)) {
    status(ctl, "Unable to save hosts");
    return 0;
  }
  status(ctl, ok);
  return 1;
}

int amiga_ctl_host_add(amiga_ctl_t *ctl, const char *uri)
{
  config_nio_state_t *s = ctl->state;

  if (!uri || !uri[0]) {
    status(ctl, "Host is empty");
    return 0;
  }
  if (s->host_count >= CONFIG_NIO_MAX_HOSTS) {
    status(ctl, "Host list is full");
    return 0;
  }
  (void) config_nio_host_set(s, s->host_count, uri);
  s->host_count++;
  amiga_list_set_count(&ctl->hosts, s->host_count);
  amiga_list_select(&ctl->hosts, (uint16_t) (s->host_count - 1));
  return save_hosts(ctl, "Host added");
}

int amiga_ctl_host_replace(amiga_ctl_t *ctl, const char *uri)
{
  uint8_t idx;

  if (!selected_host(ctl, &idx))
    return 0;
  if (!uri || !uri[0]) {
    status(ctl, "Host is empty");
    return 0;
  }
  (void) config_nio_host_set(ctl->state, idx, uri);
  return save_hosts(ctl, "Host updated");
}

int amiga_ctl_host_remove(amiga_ctl_t *ctl)
{
  config_nio_state_t *s = ctl->state;
  uint8_t idx;
  uint8_t i;

  if (!selected_host(ctl, &idx))
    return 0;
  for (i = idx; (uint8_t) (i + 1) < s->host_count; i++)
    strcpy(s->hosts[i], s->hosts[i + 1]);
  s->host_count--;
  s->hosts[s->host_count][0] = 0;
  return save_hosts(ctl, "Host removed");
}

int amiga_ctl_host_move(amiga_ctl_t *ctl, int8_t delta)
{
  static char tmp[CONFIG_NIO_URI_MAX + 1];
  config_nio_state_t *s = ctl->state;
  uint8_t idx;
  uint8_t to;

  if (!selected_host(ctl, &idx))
    return 0;
  if ((delta < 0 && idx == 0) ||
      (delta > 0 && (uint8_t) (idx + 1) >= s->host_count))
    return 0;
  to = (uint8_t) (delta < 0 ? idx - 1 : idx + 1);
  strcpy(tmp, s->hosts[idx]);
  strcpy(s->hosts[idx], s->hosts[to]);
  strcpy(s->hosts[to], tmp);
  amiga_list_select(&ctl->hosts, to);
  return save_hosts(ctl, "Host moved");
}

int amiga_ctl_set_prefs(amiga_ctl_t *ctl, uint8_t date_format,
                        uint8_t size_format)
{
  ctl->state->prefs.date_format = date_format;
  ctl->state->prefs.size_format = size_format;
  if (!config_nio_save_prefs(ctl->state)) {
    status(ctl, "Unable to save settings");
    return 0;
  }
  status(ctl, "Settings saved");
  return 1;
}

static void list_cb(uint8_t is_dir, const char *name, uint32_t size,
                    uint32_t mtime, void *ctx)
{
  config_nio_state_t *s = (config_nio_state_t *) ctx;
  config_nio_entry_t *entry;
  uint16_t n;

  s->entry_total++;
  if (s->entry_count >= CONFIG_NIO_MAX_ENTRIES) {
    s->entries_truncated = 1;
    return;
  }
  entry = &s->entries[s->entry_count++];
  entry->is_dir = is_dir;
  entry->size = size;
  entry->mtime = mtime;
  n = (uint16_t) strlen(name);
  if (n > CONFIG_NIO_NAME_MAX)
    n = CONFIG_NIO_NAME_MAX;
  memcpy(entry->name, name, n);
  entry->name[n] = 0;
}

int amiga_ctl_browse_refresh(amiga_ctl_t *ctl)
{
  static char uri[FNSVC_MAX_URI + 1];
  config_nio_state_t *s = ctl->state;

  s->entry_count = 0;
  s->entry_total = 0;
  s->entries_truncated = 0;
  ctl->browse_open = 1;
  amiga_list_set_count(&ctl->entries, 0);
  if (ctl->browse_host >= s->host_count ||
      !config_nio_compose_uri(s->hosts[ctl->browse_host], s->browse_path, "",
                              uri, sizeof(uri))) {
    status(ctl, "Path is too long");
    return 0;
  }
  if (!fnsvc_list_directory(uri, list_cb, s)) {
    s->entry_count = 0;
    amiga_sprintf(ctl->msg, "Browse failed: error %u status %u",
            (unsigned) fnsvc_last_error(), (unsigned) fnsvc_last_status());
    status(ctl, ctl->msg);
    return 0;
  }
  amiga_list_set_count(&ctl->entries, s->entry_count);
  amiga_list_select(&ctl->entries, 0);
  if (s->entries_truncated) {
    amiga_sprintf(ctl->msg, "Showing %u of %u entries",
            (unsigned) s->entry_count, (unsigned) s->entry_total);
    status(ctl, ctl->msg);
  } else {
    status(ctl, "Entries loaded");
  }
  return 1;
}

int amiga_ctl_browse_open(amiga_ctl_t *ctl)
{
  uint8_t idx;

  if (!selected_host(ctl, &idx))
    return 0;
  ctl->browse_host = idx;
  ctl->state->browse_path[0] = 0;
  ctl->page = AMIGA_PAGE_BROWSE;
  return amiga_ctl_browse_refresh(ctl);
}

static config_nio_entry_t *selected_entry(amiga_ctl_t *ctl)
{
  if (ctl->entries.selected == AMIGA_LIST_NONE ||
      ctl->entries.selected >= ctl->state->entry_count) {
    status(ctl, "Nothing selected");
    return NULL;
  }
  return &ctl->state->entries[ctl->entries.selected];
}

int amiga_ctl_browse_activate(amiga_ctl_t *ctl)
{
  config_nio_state_t *s = ctl->state;
  config_nio_entry_t *e;
  uint16_t len;
  uint16_t nlen;

  e = selected_entry(ctl);
  if (!e)
    return 0;
  if (e->is_dir & CONFIG_NIO_ENTRY_FLAG_NAME_TRUNCATED) {
    status(ctl, "Name too long for this client");
    return 0;
  }
  if (!(e->is_dir & CONFIG_NIO_ENTRY_FLAG_DIR)) {
    status(ctl, "Press Mount... to mount this image");
    return 0;
  }
  len = (uint16_t) strlen(s->browse_path);
  nlen = (uint16_t) strlen(e->name);
  if ((uint16_t) (len + nlen + 2) > CONFIG_NIO_PATH_MAX) {
    status(ctl, "Path is too long");
    return 0;
  }
  memcpy(&s->browse_path[len], e->name, nlen);
  s->browse_path[len + nlen] = '/';
  s->browse_path[len + nlen + 1] = 0;
  return amiga_ctl_browse_refresh(ctl);
}

int amiga_ctl_browse_parent(amiga_ctl_t *ctl)
{
  char *path = ctl->state->browse_path;
  uint16_t len;

  len = (uint16_t) strlen(path);
  if (len == 0) {
    status(ctl, "Already at the top");
    return 0;
  }
  while (len > 0 && path[len - 1] == '/')
    path[--len] = 0;
  while (len > 0 && path[len - 1] != '/')
    path[--len] = 0;
  return amiga_ctl_browse_refresh(ctl);
}

int amiga_ctl_browse_select_name(amiga_ctl_t *ctl, const char *name)
{
  uint8_t i;

  for (i = 0; i < ctl->state->entry_count; i++) {
    if (!strcmp(ctl->state->entries[i].name, name)) {
      amiga_list_select(&ctl->entries, i);
      return 1;
    }
  }
  status(ctl, "No such entry");
  return 0;
}

int amiga_ctl_browse_uri(amiga_ctl_t *ctl, char *out, uint16_t cap)
{
  config_nio_entry_t *e = selected_entry(ctl);

  if (!e)
    return 0;
  if (e->is_dir & CONFIG_NIO_ENTRY_FLAG_NAME_TRUNCATED) {
    status(ctl, "Name too long for this client");
    return 0;
  }
  if (e->is_dir & CONFIG_NIO_ENTRY_FLAG_DIR) {
    status(ctl, "Pick a file, not a drawer");
    return 0;
  }
  if (!config_nio_compose_uri(ctl->state->hosts[ctl->browse_host],
                              ctl->state->browse_path, e->name, out, cap)) {
    status(ctl, "URI is too long");
    return 0;
  }
  return 1;
}

int amiga_ctl_browse_assign(amiga_ctl_t *ctl, uint8_t slot, uint8_t readonly)
{
  static char uri[FNSVC_MAX_URI + 1];

  if (!amiga_ctl_browse_uri(ctl, uri, sizeof(uri)))
    return 0;
  if (!config_nio_write_slot(ctl->state, slot, uri, readonly ? "r" : "rw")) {
    status(ctl, "Unable to save slot");
    return 0;
  }
  cat_invalidate(ctl);
  amiga_sprintf(ctl->msg, "Assigned to slot %u", (unsigned) slot);
  status(ctl, ctl->msg);
  return 1;
}

static int find_slot(amiga_ctl_t *ctl, const char *uri, uint8_t *slot);

/* FIN without a slot number: the slot already holding the image, else the
 * first empty one. */
int amiga_ctl_browse_add(amiga_ctl_t *ctl, uint8_t readonly)
{
  static char uri[FNSVC_MAX_URI + 1];
  const config_nio_slot_t *s;
  const char *name;
  const char *mode = readonly ? "RO" : "RW";
  uint8_t slot;

  if (!amiga_ctl_browse_uri(ctl, uri, sizeof(uri)) ||
      !find_slot(ctl, uri, &slot))
    return 0;
  name = strrchr(uri, '/');
  name = name && name[1] ? name + 1 : uri;
  s = amiga_ctl_slot(ctl, slot);
  if (s && s->enabled && strcmp(s->uri, uri) == 0 &&
      (strcmp(s->mode, "r") == 0) == (readonly != 0)) {
    amiga_sprintf(ctl->msg, "%.40s is already in slot %u (%s)", name,
                  (unsigned) slot, mode);
    status(ctl, ctl->msg);
    return 1;
  }
  cat_invalidate(ctl);
  if (!config_nio_write_slot(ctl->state, slot, uri, readonly ? "r" : "rw")) {
    status(ctl, "Unable to save slot");
    return 0;
  }
  amiga_sprintf(ctl->msg, "%.40s added to slot %u (%s)", name, (unsigned) slot,
                mode);
  status(ctl, ctl->msg);
  return 1;
}

const config_nio_slot_t *amiga_ctl_slot(amiga_ctl_t *ctl, uint8_t slot)
{
  uint16_t base;
  uint8_t w;
  uint8_t i;

  base = (uint16_t) (slot & ~(AMIGA_CAT_WINDOW - 1));
  for (w = 0; w < 2; w++) {
    if (ctl->cat_base[w] == base)
      return &ctl->cat[w][slot - base];
  }
  w = ctl->cat_victim;
  ctl->cat_victim = (uint8_t) (w ^ 1);
  for (i = 0; i < AMIGA_CAT_WINDOW; i++) {
    /* config_nio_read_slot: 1 = entry present, or missing (zeroed);
     * 0 = transport/format error. */
    if (!config_nio_read_slot((uint8_t) (base + i), &ctl->cat[w][i])) {
      ctl->cat_base[w] = AMIGA_CAT_SLOTS;
      status(ctl, "Unable to read catalogue");
      return NULL;
    }
  }
  ctl->cat_base[w] = base;
  return &ctl->cat[w][slot - base];
}

int amiga_ctl_slot_set(amiga_ctl_t *ctl, uint8_t slot, const char *uri,
                       uint8_t readonly)
{
  if (!uri || !uri[0]) {
    status(ctl, "URI is empty");
    return 0;
  }
  cat_invalidate(ctl);
  if (!config_nio_write_slot(ctl->state, slot, uri, readonly ? "r" : "rw")) {
    status(ctl, "Unable to save slot");
    return 0;
  }
  amiga_sprintf(ctl->msg, "Slot %u saved", (unsigned) slot);
  status(ctl, ctl->msg);
  return 1;
}

int amiga_ctl_slot_clear(amiga_ctl_t *ctl, uint8_t slot)
{
  cat_invalidate(ctl);
  if (!config_nio_delete_slot(ctl->state, slot)) {
    status(ctl, "Unable to clear slot");
    return 0;
  }
  amiga_sprintf(ctl->msg, "Slot %u cleared", (unsigned) slot);
  status(ctl, ctl->msg);
  return 1;
}

int amiga_ctl_reload(amiga_ctl_t *ctl)
{
  static char path[CONFIG_NIO_PATH_MAX + 1];
  config_nio_state_t *s = ctl->state;

  strcpy(path, s->browse_path);
  if (!config_nio_load(s)) {
    status(ctl, "Unable to reload FujiNet state");
    return 0;
  }
  strcpy(s->browse_path, path);
  /* config_nio_load() clears the directory listing; Browse re-reads it. */
  ctl->browse_open = 0;
  amiga_list_set_count(&ctl->entries, 0);
  amiga_list_set_count(&ctl->hosts, s->host_count);
  cat_invalidate(ctl);
  return 1;
}

/* Reports a failed FMOUNT/FUMOUNT with the command's own last message. */
static void command_failed(amiga_ctl_t *ctl, const char *tool,
                           const char *label, int rc)
{
  if (ctl->cmd_out[0])
    amiga_sprintf(ctl->msg, "%s failed for %s %.50s (rc %d)", tool, label,
            ctl->cmd_out, rc);
  else
    amiga_sprintf(ctl->msg, "%s failed for %s (rc %d)", tool, label, rc);
  status(ctl, ctl->msg);
}

int amiga_ctl_drive_insert(amiga_ctl_t *ctl, uint8_t unit, uint8_t slot,
                           uint8_t readonly)
{
  const config_nio_slot_t *entry;
  const char *label;
  config_nio_mapping_t m;
  int rc;

  label = amiga_drive_label(unit, ctl->kick13);
  if (!label) {
    status(ctl, "No such drive");
    return 0;
  }
  entry = amiga_ctl_slot(ctl, slot);
  if (!entry)
    return 0;
  if (!entry->enabled || !entry->uri[0]) {
    amiga_sprintf(ctl->msg, "Catalogue slot %u is empty", (unsigned) slot);
    status(ctl, ctl->msg);
    return 0;
  }
  if (!amiga_fmount_command(ctl->cmd, sizeof(ctl->cmd), ctl->fmount, slot,
                            unit, ctl->kick13, readonly)) {
    status(ctl, "FMOUNT path is invalid");
    return 0;
  }
  ctl->cmd_out[0] = 0;
  rc = ctl->exec(ctl->cmd, ctl->cmd_out, sizeof(ctl->cmd_out), ctl->exec_ctx);
  if (!amiga_ctl_reload(ctl))
    return 0;
  if (rc != 0 || !config_nio_mapping_get(ctl->state, unit, &m) ||
      !m.valid || m.slot != slot) {
    command_failed(ctl, "FMOUNT", label, rc);
    return 0;
  }
  amiga_sprintf(ctl->msg, "Slot %u inserted in %s", (unsigned) slot, label);
  status(ctl, ctl->msg);
  return 1;
}

int amiga_ctl_drive_eject(amiga_ctl_t *ctl, uint8_t unit)
{
  const char *label;
  config_nio_mapping_t m;
  int rc;

  label = amiga_drive_label(unit, ctl->kick13);
  if (!label) {
    status(ctl, "No such drive");
    return 0;
  }
  if (!config_nio_mapping_get(ctl->state, unit, &m) || !m.valid) {
    amiga_sprintf(ctl->msg, "%s is empty", label);
    status(ctl, ctl->msg);
    return 0;
  }
  if (!amiga_fumount_command(ctl->cmd, sizeof(ctl->cmd), ctl->fumount, unit,
                             ctl->kick13)) {
    status(ctl, "FUMOUNT path is invalid");
    return 0;
  }
  ctl->cmd_out[0] = 0;
  rc = ctl->exec(ctl->cmd, ctl->cmd_out, sizeof(ctl->cmd_out), ctl->exec_ctx);
  if (!amiga_ctl_reload(ctl))
    return 0;
  if (rc != 0 || !config_nio_mapping_get(ctl->state, unit, &m) || m.valid) {
    command_failed(ctl, "FUMOUNT", label, rc);
    return 0;
  }
  amiga_sprintf(ctl->msg, "%s ejected", label);
  status(ctl, ctl->msg);
  return 1;
}

const config_nio_slot_t *amiga_ctl_drive_slot(amiga_ctl_t *ctl, uint8_t unit)
{
  config_nio_mapping_t m;

  if (unit >= AMIGA_DRIVE_COUNT ||
      !config_nio_mapping_get(ctl->state, unit, &m) || !m.valid)
    return NULL;
  if (!(ctl->drive_cached & (1u << unit))) {
    if (!config_nio_read_slot(m.slot, &ctl->drive_cat[unit])) {
      status(ctl, "Unable to read catalogue");
      return NULL;
    }
    ctl->drive_cached = (uint8_t) (ctl->drive_cached | (1u << unit));
  }
  return &ctl->drive_cat[unit];
}

uint8_t amiga_ctl_catalogue_readonly(amiga_ctl_t *ctl, uint8_t slot)
{
  const config_nio_slot_t *s = amiga_ctl_slot(ctl, slot);

  if (!s || !s->enabled || !s->uri[0])
    return 1;
  return (uint8_t) (strcmp(s->mode, "r") == 0);
}

void amiga_ctl_set_probe(amiga_ctl_t *ctl, amiga_probe_fn probe, void *ctx)
{
  ctl->probe = probe;
  ctl->probe_ctx = ctx;
}

uint8_t amiga_ctl_drive_state(amiga_ctl_t *ctl, uint8_t unit)
{
  config_nio_mapping_t m;

  if (unit >= AMIGA_DRIVE_COUNT ||
      !config_nio_mapping_get(ctl->state, unit, &m) || !m.valid)
    return AMIGA_DRIVE_EMPTY;
  if (ctl->probe && !ctl->probe(unit, ctl->probe_ctx))
    return AMIGA_DRIVE_SAVED;
  return AMIGA_DRIVE_MOUNTED;
}

int amiga_ctl_drive_mounted(amiga_ctl_t *ctl, uint8_t unit)
{
  return amiga_ctl_drive_state(ctl, unit) == AMIGA_DRIVE_MOUNTED;
}

uint8_t amiga_ctl_first_empty_drive(amiga_ctl_t *ctl)
{
  uint8_t unit;

  for (unit = 0; unit < AMIGA_DRIVE_COUNT; unit++) {
    if (amiga_ctl_drive_state(ctl, unit) == AMIGA_DRIVE_EMPTY)
      return unit;
  }
  for (unit = 0; unit < AMIGA_DRIVE_COUNT; unit++) {
    if (amiga_ctl_drive_state(ctl, unit) == AMIGA_DRIVE_SAVED)
      return unit;
  }
  return 0;
}

int amiga_ctl_drive_remount(amiga_ctl_t *ctl, uint8_t unit)
{
  const char *label = amiga_drive_label(unit, ctl->kick13);
  config_nio_mapping_t m;

  if (amiga_ctl_drive_state(ctl, unit) != AMIGA_DRIVE_SAVED ||
      !config_nio_mapping_get(ctl->state, unit, &m)) {
    amiga_sprintf(ctl->msg, "%s has nothing to remount", label ? label : "Drive");
    status(ctl, ctl->msg);
    return 0;
  }
  if (!amiga_ctl_drive_insert(ctl, unit, m.slot, m.readonly))
    return 0;
  {
    const config_nio_slot_t *slot = amiga_ctl_drive_slot(ctl, unit);
    const char *name = slot && slot->enabled ? strrchr(slot->uri, '/') : NULL;

    name = name && name[1] ? name + 1 : (slot && slot->enabled ? slot->uri
                                                               : "Image");
    amiga_sprintf(ctl->msg, "%.40s mounted on %s (%s, slot %u)", name, label,
                  m.readonly ? "RO" : "RW", (unsigned) m.slot);
    status(ctl, ctl->msg);
  }
  return 1;
}

static void set_mount_name(amiga_ctl_t *ctl, const char *uri)
{
  const char *name = strrchr(uri, '/');

  name = name && name[1] ? name + 1 : uri;
  strncpy(ctl->mount_name, name, sizeof(ctl->mount_name) - 1);
  ctl->mount_name[sizeof(ctl->mount_name) - 1] = 0;
}

static void enter_mount(amiga_ctl_t *ctl)
{
  ctl->mount_return = ctl->page;
  ctl->page = AMIGA_PAGE_MOUNT;
  amiga_list_select(&ctl->drives, amiga_ctl_first_empty_drive(ctl));
  amiga_sprintf(ctl->msg, "Choose a drive for %s", ctl->mount_name);
  status(ctl, ctl->msg);
}

int amiga_ctl_mount_begin_browse(amiga_ctl_t *ctl)
{
  if (!amiga_ctl_browse_uri(ctl, ctl->mount_uri, sizeof(ctl->mount_uri)))
    return 0;
  ctl->mount_slot = -1;
  set_mount_name(ctl, ctl->mount_uri);
  enter_mount(ctl);
  return 1;
}

int amiga_ctl_mount_begin_slot(amiga_ctl_t *ctl, uint8_t slot)
{
  const config_nio_slot_t *s = amiga_ctl_slot(ctl, slot);

  if (!s)
    return 0;
  if (!s->enabled || !s->uri[0]) {
    amiga_sprintf(ctl->msg, "Catalogue slot %u is empty", (unsigned) slot);
    status(ctl, ctl->msg);
    return 0;
  }
  strcpy(ctl->mount_uri, s->uri);
  ctl->mount_slot = slot;
  set_mount_name(ctl, ctl->mount_uri);
  enter_mount(ctl);
  return 1;
}

void amiga_ctl_mount_cancel(amiga_ctl_t *ctl)
{
  if (ctl->page == AMIGA_PAGE_MOUNT)
    ctl->page = ctl->mount_return;
  status(ctl, "Mount cancelled");
}

/* The slot already holding `uri`, else the first empty one.  Range reads
 * page through the occupied slots instead of reading all 256. */
static int find_slot(amiga_ctl_t *ctl, const char *uri, uint8_t *slot)
{
  static uint8_t used[AMIGA_CAT_SLOTS / 8];
  fn_slot_catalog_page_t page;
  uint16_t want = (uint16_t) strlen(uri);
  uint8_t cursor = 0;
  uint16_t i;

  memset(used, 0, sizeof(used));
  for (;;) {
    uint16_t off = 0;

    if (fn_slot_catalog_range(&cat_io, 0, 255, cursor, 0,
                              (uint8_t) CONFIG_NIO_URI_MAX,
                              (uint16_t) (sizeof(cat_io_buf) - 7),
                              &page) != FN_OK) {
      status(ctl, "Unable to read catalogue");
      return 0;
    }
    for (i = 0; i < page.entry_count; i++) {
      fn_slot_catalog_entry_t e;

      if (fn_slot_catalog_next_entry(&page, &off, &e) != FN_OK)
        break;
      if (!(e.flags & FN_SLOT_CATALOG_ENTRY_VALID))
        continue;
      used[e.index / 8] = (uint8_t) (used[e.index / 8] | (1u << (e.index % 8)));
      if (e.uri_len == want && memcmp(e.uri, uri, want) == 0 &&
          !(e.flags & FN_SLOT_CATALOG_ENTRY_URI_TRUNCATED)) {
        *slot = e.index;
        return 1;
      }
    }
    if (!(page.flags & FN_SLOT_CATALOG_MORE) || page.next_index <= cursor)
      break;
    cursor = page.next_index;
  }
  for (i = 0; i < AMIGA_CAT_SLOTS; i++) {
    if (!(used[i / 8] & (1u << (i % 8)))) {
      *slot = (uint8_t) i;
      return 1;
    }
  }
  status(ctl, "Catalogue is full");
  return 0;
}

int amiga_ctl_catalogue_refresh(amiga_ctl_t *ctl)
{
  fn_slot_catalog_page_t page;
  uint8_t cursor = 0;
  uint16_t i;

  ctl->cat_count = 0;
  for (;;) {
    uint16_t off = 0;

    if (fn_slot_catalog_range(&cat_io, 0, 255, cursor,
                              FN_SLOT_CATALOG_TAIL_URI,
                              (uint8_t) AMIGA_CAT_URI_MAX,
                              (uint16_t) (sizeof(cat_io_buf) - 7),
                              &page) != FN_OK) {
      amiga_list_set_count(&ctl->catalogue, ctl->cat_count);
      status(ctl, "Unable to read catalogue");
      return 0;
    }
    for (i = 0; i < page.entry_count; i++) {
      fn_slot_catalog_entry_t e;
      char *dst;
      uint16_t len;

      if (fn_slot_catalog_next_entry(&page, &off, &e) != FN_OK)
        break;
      if (!(e.flags & FN_SLOT_CATALOG_ENTRY_VALID) ||
          ctl->cat_count >= AMIGA_CAT_SLOTS)
        continue;
      ctl->cat_slot[ctl->cat_count] = e.index;
      ctl->cat_ro[ctl->cat_count] =
        (uint8_t) ((e.flags & FN_SLOT_CATALOG_ENTRY_READ_ONLY) != 0);
      dst = ctl->cat_uri[ctl->cat_count];
      len = e.uri_len;
      if (e.flags & FN_SLOT_CATALOG_ENTRY_URI_TRUNCATED) {
        /* Only the tail came back: mark the cut. */
        if (len > AMIGA_CAT_URI_MAX - 3)
          len = AMIGA_CAT_URI_MAX - 3;
        strcpy(dst, "...");
        memcpy(dst + 3, e.uri + e.uri_len - len, len);
        dst[3 + len] = 0;
      } else {
        if (len > AMIGA_CAT_URI_MAX)
          len = AMIGA_CAT_URI_MAX;
        memcpy(dst, e.uri, len);
        dst[len] = 0;
      }
      ctl->cat_count++;
    }
    if (!(page.flags & FN_SLOT_CATALOG_MORE) || page.next_index <= cursor)
      break;
    cursor = page.next_index;
  }
  amiga_list_set_count(&ctl->catalogue, ctl->cat_count);
  return 1;
}

int amiga_ctl_catalogue_index(amiga_ctl_t *ctl, uint8_t slot)
{
  uint16_t i;

  for (i = 0; i < ctl->cat_count; i++) {
    if (ctl->cat_slot[i] == slot)
      return (int) i;
  }
  return -1;
}

int amiga_ctl_mount_commit(amiga_ctl_t *ctl, uint8_t unit, uint8_t readonly)
{
  const config_nio_slot_t *s;
  const char *label;
  uint8_t slot;
  uint8_t return_page = ctl->mount_return;

  if (ctl->mount_slot >= 0)
    slot = (uint8_t) ctl->mount_slot;
  else if (!find_slot(ctl, ctl->mount_uri, &slot))
    return 0;
  s = amiga_ctl_slot(ctl, slot);
  if (!s)
    return 0;
  if (!s->enabled || strcmp(s->uri, ctl->mount_uri) != 0 ||
      (strcmp(s->mode, "r") == 0) != (readonly != 0)) {
    if (!amiga_ctl_slot_set(ctl, slot, ctl->mount_uri, readonly))
      return 0;
  }
  if (!amiga_ctl_drive_insert(ctl, unit, slot, readonly))
    return 0;
  label = amiga_drive_label(unit, ctl->kick13);
  ctl->page = return_page;
  amiga_sprintf(ctl->msg, "%s mounted on %s (%s, slot %u)", ctl->mount_name,
                label, readonly ? "RO" : "RW", (unsigned) slot);
  status(ctl, ctl->msg);
  return 1;
}

void amiga_ctl_help_open(amiga_ctl_t *ctl, uint8_t topic)
{
  if (ctl->page != AMIGA_PAGE_HELP)
    ctl->help_return = ctl->page;
  ctl->page = AMIGA_PAGE_HELP;
  ctl->help_topic = topic < AMIGA_HELP_TOPICS ? topic : AMIGA_HELP_CONTENTS;
}

void amiga_ctl_help_step(amiga_ctl_t *ctl, int8_t delta)
{
  int16_t topic = (int16_t) ctl->help_topic + delta;

  if (topic < 1)
    topic = 1;
  if (topic >= AMIGA_HELP_TOPICS)
    topic = AMIGA_HELP_TOPICS - 1;
  amiga_ctl_help_open(ctl, (uint8_t) topic);
}

void amiga_ctl_help_close(amiga_ctl_t *ctl)
{
  if (ctl->page == AMIGA_PAGE_HELP) {
    ctl->page = ctl->help_return;
    status(ctl, "Ready");
  }
}

int amiga_ctl_drive_window_name(amiga_ctl_t *ctl, uint8_t unit, char *out,
                                uint16_t cap)
{
  const char *label = amiga_drive_label(unit, ctl->kick13);

  if (!label || cap <= strlen(label)) {
    status(ctl, "No such drive");
    return 0;
  }
  switch (amiga_ctl_drive_state(ctl, unit)) {
  case AMIGA_DRIVE_MOUNTED:
    break;
  case AMIGA_DRIVE_SAVED:
    amiga_sprintf(ctl->msg, "%s is not mounted; press Remount", label);
    status(ctl, ctl->msg);
    return 0;
  default:
    amiga_sprintf(ctl->msg, "%s is empty", label);
    status(ctl, ctl->msg);
    return 0;
  }
  strcpy(out, label);
  return 1;
}

