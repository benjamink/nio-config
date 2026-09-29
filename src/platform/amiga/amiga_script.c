#include "amiga_script.h"
#include "amiga_fmt.h"
#include "amiga_drives.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOKENS 5

static char line_buf[CONFIG_NIO_URI_MAX + 64];
static char out_buf[CONFIG_NIO_URI_MAX + 32];

static int split(char *s, char **tok)
{
  int n = 0;

  while (*s && n < MAX_TOKENS) {
    while (*s == ' ' || *s == '\t')
      *s++ = 0;
    if (!*s)
      break;
    tok[n++] = s;
    while (*s && *s != ' ' && *s != '\t')
      s++;
  }
  return n;
}

/* Case-insensitive match against a lower-case keyword. */
static int is(const char *a, const char *b)
{
  while (*a && *b) {
    char c = *a;
    if (c >= 'A' && c <= 'Z')
      c = (char) (c + 32);
    if (c != *b)
      return 0;
    a++;
    b++;
  }
  return *a == 0 && *b == 0;
}

static int parse_mode(const char *s, uint8_t *readonly)
{
  if (is(s, "ro")) {
    *readonly = 1;
    return 1;
  }
  if (is(s, "rw")) {
    *readonly = 0;
    return 1;
  }
  return 0;
}

static int parse_u8(const char *s, uint8_t *v)
{
  char *end;
  long n;

  n = strtol(s, &end, 10);
  if (!*s || *end || n < 0 || n > 255)
    return 0;
  *v = (uint8_t) n;
  return 1;
}

static int result(amiga_ctl_t *ctl, int ok, amiga_script_out_fn out,
                  void *ctx)
{
  if (ok) {
    out("OK", ctx);
    return AMIGA_SCRIPT_OK;
  }
  amiga_sprintf(out_buf, "ERR %s", ctl->state->status);
  out(out_buf, ctx);
  return AMIGA_SCRIPT_ERR;
}

static int fail(amiga_ctl_t *ctl, const char *msg, amiga_script_out_fn out,
                void *ctx)
{
  config_nio_set_status(ctl->state, msg);
  return result(ctl, 0, out, ctx);
}

static int dump(amiga_ctl_t *ctl, char **t, int n, amiga_script_out_fn out,
                void *ctx)
{
  config_nio_state_t *s = ctl->state;
  uint8_t i;

  if (n == 2 && is(t[1], "hosts")) {
    for (i = 0; i < s->host_count; i++) {
      amiga_sprintf(out_buf, "HOST %u %s", (unsigned) i, s->hosts[i]);
      out(out_buf, ctx);
    }
  } else if (n == 2 && is(t[1], "entries")) {
    for (i = 0; i < s->entry_count; i++) {
      amiga_sprintf(out_buf, "ENTRY %c %s %lu",
              (s->entries[i].is_dir & CONFIG_NIO_ENTRY_FLAG_DIR) ? 'D' : 'F',
              s->entries[i].name, (unsigned long) s->entries[i].size);
      out(out_buf, ctx);
    }
  } else if (n == 2 && is(t[1], "drives")) {
    for (i = 0; i < AMIGA_DRIVE_COUNT; i++) {
      config_nio_mapping_t m;

      if (config_nio_mapping_get(s, i, &m) && m.valid)
        amiga_sprintf(out_buf, "DRIVE %s %u %s", amiga_drive_label(i, ctl->kick13),
                (unsigned) m.slot, m.readonly ? "RO" : "RW");
      else
        amiga_sprintf(out_buf, "DRIVE %s - -", amiga_drive_label(i, ctl->kick13));
      out(out_buf, ctx);
    }
  } else if (n == 3 && is(t[1], "slot") && parse_u8(t[2], &i)) {
    const config_nio_slot_t *slot = amiga_ctl_slot(ctl, i);

    if (!slot)
      return result(ctl, 0, out, ctx);
    if (slot->enabled && slot->uri[0])
      amiga_sprintf(out_buf, "SLOT %u %s %s", (unsigned) i,
              strcmp(slot->mode, "r") == 0 ? "RO" : "RW", slot->uri);
    else
      amiga_sprintf(out_buf, "SLOT %u EMPTY", (unsigned) i);
    out(out_buf, ctx);
  } else if (n == 2 && is(t[1], "catalogue")) {
    uint16_t k;

    if (!amiga_ctl_catalogue_refresh(ctl))
      return result(ctl, 0, out, ctx);
    for (k = 0; k < ctl->cat_count; k++) {
      amiga_sprintf(out_buf, "SLOT %u %s %s", (unsigned) ctl->cat_slot[k],
                    ctl->cat_ro[k] ? "RO" : "RW", ctl->cat_uri[k]);
      out(out_buf, ctx);
    }
  } else if (n == 2 && is(t[1], "status")) {
    amiga_sprintf(out_buf, "STATUS %s", s->status);
    out(out_buf, ctx);
  } else {
    return fail(ctl, "Bad arguments", out, ctx);
  }
  return result(ctl, 1, out, ctx);
}

static int host_command(amiga_ctl_t *ctl, char **t, int n,
                        amiga_script_out_fn out, void *ctx)
{
  uint8_t a;

  if (is(t[1], "add") && n == 3)
    return result(ctl, amiga_ctl_host_add(ctl, t[2]), out, ctx);
  if (is(t[1], "edit") && n == 3)
    return result(ctl, amiga_ctl_host_replace(ctl, t[2]), out, ctx);
  if (is(t[1], "remove") && n == 2)
    return result(ctl, amiga_ctl_host_remove(ctl), out, ctx);
  if (is(t[1], "up") && n == 2)
    return result(ctl, amiga_ctl_host_move(ctl, -1), out, ctx);
  if (is(t[1], "down") && n == 2)
    return result(ctl, amiga_ctl_host_move(ctl, 1), out, ctx);
  if (is(t[1], "select") && n == 3 && parse_u8(t[2], &a)) {
    if (a >= ctl->state->host_count)
      return fail(ctl, "No host selected", out, ctx);
    amiga_list_select(&ctl->hosts, a);
    return result(ctl, 1, out, ctx);
  }
  return fail(ctl, "Bad arguments", out, ctx);
}

int amiga_script_line(amiga_ctl_t *ctl, const char *line,
                      amiga_script_out_fn out, void *out_ctx,
                      uint16_t *wait_ticks)
{
  /* Pages a script may show; the mount picker is entered via "mount". */
  static const char *const pages[AMIGA_PAGE_MOUNT] = {
    "hosts", "browse", "catalogue", "drives"
  };
  char *t[MAX_TOKENS];
  int n;
  int unit;
  uint8_t a;
  uint8_t ro;

  strncpy(line_buf, line ? line : "", sizeof(line_buf) - 1);
  line_buf[sizeof(line_buf) - 1] = 0;
  n = split(line_buf, t);
  if (n == 0 || t[0][0] == ';')
    return AMIGA_SCRIPT_OK;

  if (is(t[0], "quit") && n == 1)
    return AMIGA_SCRIPT_QUIT;
  if (is(t[0], "wait") && n == 2) {
    *wait_ticks = (uint16_t) atoi(t[1]);
    return AMIGA_SCRIPT_WAIT;
  }
  if (is(t[0], "page") && n == 2) {
    for (a = 0; a < AMIGA_PAGE_MOUNT; a++) {
      if (is(t[1], pages[a])) {
        amiga_ctl_set_page(ctl, a);
        return result(ctl, 1, out, out_ctx);
      }
    }
    return fail(ctl, "Unknown page", out, out_ctx);
  }
  if (is(t[0], "host") && n >= 2)
    return host_command(ctl, t, n, out, out_ctx);
  if (is(t[0], "browse") && n == 1)
    return result(ctl, amiga_ctl_browse_open(ctl), out, out_ctx);
  if (is(t[0], "select") && n == 2)
    return result(ctl, amiga_ctl_browse_select_name(ctl, t[1]), out, out_ctx);
  if (is(t[0], "enter") && n == 1)
    return result(ctl, amiga_ctl_browse_activate(ctl), out, out_ctx);
  if (is(t[0], "parent") && n == 1)
    return result(ctl, amiga_ctl_browse_parent(ctl), out, out_ctx);
  if (is(t[0], "assign") && n == 3 && parse_u8(t[1], &a) &&
      parse_mode(t[2], &ro))
    return result(ctl, amiga_ctl_browse_assign(ctl, a, ro), out, out_ctx);
  if (is(t[0], "slot") && n == 5 && is(t[1], "set") && parse_u8(t[2], &a) &&
      parse_mode(t[4], &ro))
    return result(ctl, amiga_ctl_slot_set(ctl, a, t[3], ro), out, out_ctx);
  if (is(t[0], "slot") && n == 3 && is(t[1], "clear") && parse_u8(t[2], &a))
    return result(ctl, amiga_ctl_slot_clear(ctl, a), out, out_ctx);
  if (is(t[0], "insert") && n == 4 && parse_u8(t[1], &a) &&
      parse_mode(t[3], &ro)) {
    unit = amiga_drive_unit(t[2], ctl->kick13);
    if (unit < 0)
      return fail(ctl, "No such drive", out, out_ctx);
    return result(ctl, amiga_ctl_drive_insert(ctl, (uint8_t) unit, a, ro),
                  out, out_ctx);
  }
  if (is(t[0], "mount") && n == 3 && parse_mode(t[2], &ro)) {
    unit = amiga_drive_unit(t[1], ctl->kick13);
    if (unit < 0)
      return fail(ctl, "No such drive", out, out_ctx);
    if (!amiga_ctl_mount_begin_browse(ctl))
      return result(ctl, 0, out, out_ctx);
    if (!amiga_ctl_mount_commit(ctl, (uint8_t) unit, ro)) {
      ctl->page = ctl->mount_return;   /* keep the failure status */
      return result(ctl, 0, out, out_ctx);
    }
    return result(ctl, 1, out, out_ctx);
  }
  if (is(t[0], "eject") && n == 2) {
    unit = amiga_drive_unit(t[1], ctl->kick13);
    if (unit < 0)
      return fail(ctl, "No such drive", out, out_ctx);
    return result(ctl, amiga_ctl_drive_eject(ctl, (uint8_t) unit), out,
                  out_ctx);
  }
  if (is(t[0], "dump"))
    return dump(ctl, t, n, out, out_ctx);
  if (is(t[0], "insert") || is(t[0], "eject") || is(t[0], "assign") ||
      is(t[0], "slot") || is(t[0], "page") || is(t[0], "wait") ||
      is(t[0], "quit") || is(t[0], "browse") || is(t[0], "select") ||
      is(t[0], "enter") || is(t[0], "parent") || is(t[0], "mount"))
    return fail(ctl, "Bad arguments", out, out_ctx);
  return fail(ctl, "Unknown command", out, out_ctx);
}
