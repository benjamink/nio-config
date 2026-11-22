#include "amiga_launch.h"

#include <string.h>

static char lower(char c)
{
  return (char) (c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
}

static int name_cmp(const char *a, const char *b)
{
  while (*a && lower(*a) == lower(*b)) {
    a++;
    b++;
  }
  return (unsigned char) lower(*a) - (unsigned char) lower(*b);
}

/* Drawers first, then programs, each in name order. */
static int entry_less(const amiga_launch_entry_t *a, const amiga_launch_entry_t *b)
{
  if (a->is_dir != b->is_dir)
    return a->is_dir > b->is_dir;
  return name_cmp(a->name, b->name) < 0;
}

static void sort_entries(amiga_launch_entry_t *e, uint16_t n)
{
  uint16_t i;

  for (i = 1; i < n; i++) {
    amiga_launch_entry_t key = e[i];
    int16_t j = (int16_t) (i - 1);

    while (j >= 0 && entry_less(&key, &e[j])) {
      e[j + 1] = e[j];
      j--;
    }
    e[j + 1] = key;
  }
}

void amiga_launch_init(amiga_launch_t *l, uint8_t rows,
                       amiga_launch_scan_fn scan, void *scan_ctx)
{
  memset(l, 0, sizeof(*l));
  l->scan = scan;
  l->scan_ctx = scan_ctx;
  amiga_list_init(&l->list, rows);
}

uint16_t amiga_launch_rows(const amiga_launch_t *l)
{
  return (uint16_t) (l->count + (l->depth ? 1 : 0));
}

int amiga_launch_row_is_parent(const amiga_launch_t *l, uint16_t row)
{
  return l->depth && row == 0;
}

const amiga_launch_entry_t *amiga_launch_row_entry(const amiga_launch_t *l,
                                                   uint16_t row)
{
  uint16_t i = (uint16_t) (row - (l->depth ? 1 : 0));

  if (amiga_launch_row_is_parent(l, row) || i >= l->count)
    return NULL;
  return &l->entries[i];
}

/* Reads `path` into the list, keeping only drawers and programs. */
static int load(amiga_launch_t *l, const char *path)
{
  static amiga_launch_entry_t raw[AMIGA_LAUNCH_MAX];
  uint16_t n = 0;
  uint16_t i;
  uint16_t kept = 0;

  l->error = NULL;
  if (!l->scan || !l->scan(path, raw, AMIGA_LAUNCH_MAX, &n, l->scan_ctx)) {
    l->error = "Cannot read the drive";
    return 0;
  }
  for (i = 0; i < n; i++) {
    if (raw[i].is_dir || raw[i].is_exe)
      l->entries[kept++] = raw[i];
  }
  sort_entries(l->entries, kept);
  l->count = kept;
  l->truncated = n >= AMIGA_LAUNCH_MAX;
  strcpy(l->path, path);
  amiga_list_set_count(&l->list, amiga_launch_rows(l));
  /* Start on the first program (past the ".." and drawers), else the top. */
  for (i = 0; i < kept && l->entries[i].is_dir; i++)
    ;
  amiga_list_select(&l->list,
                    (uint16_t) ((i < kept ? i : 0) + (l->depth ? 1 : 0)));
  return 1;
}

int amiga_launch_open(amiga_launch_t *l, const char *root)
{
  if (strlen(root) >= AMIGA_LAUNCH_PATH_MAX) {
    l->error = "Drive name too long";
    return 0;
  }
  l->depth = 0;
  l->count = 0;
  return load(l, root);
}

int amiga_launch_activate(amiga_launch_t *l, uint16_t row)
{
  const amiga_launch_entry_t *e;
  char next[AMIGA_LAUNCH_PATH_MAX];
  size_t len;

  if (amiga_launch_row_is_parent(l, row))
    return amiga_launch_parent(l) ? AMIGA_LAUNCH_ENTERED : AMIGA_LAUNCH_FAILED;
  e = amiga_launch_row_entry(l, row);
  if (!e)
    return AMIGA_LAUNCH_FAILED;
  if (!e->is_dir)
    return AMIGA_LAUNCH_PROGRAM;
  len = strlen(l->path);
  if (len + strlen(e->name) + 2 > sizeof(next)) {
    l->error = "Path too long";
    return AMIGA_LAUNCH_FAILED;
  }
  strcpy(next, l->path);
  strcat(next, e->name);
  strcat(next, "/");
  l->depth++;
  if (!load(l, next)) {
    l->depth--;
    return AMIGA_LAUNCH_FAILED;
  }
  return AMIGA_LAUNCH_ENTERED;
}

int amiga_launch_parent(amiga_launch_t *l)
{
  char up[AMIGA_LAUNCH_PATH_MAX];
  size_t len = strlen(l->path);

  if (!l->depth || len == 0)
    return 0;
  strcpy(up, l->path);
  /* Drop the trailing '/' of this drawer, then its name. */
  up[--len] = 0;
  while (len > 0 && up[len - 1] != '/' && up[len - 1] != ':')
    up[--len] = 0;
  l->depth--;
  if (!load(l, up)) {
    l->depth++;
    return 0;
  }
  return 1;
}

int amiga_launch_command(const amiga_launch_t *l, uint16_t row,
                         const char *args, char *out, uint16_t cap)
{
  const amiga_launch_entry_t *e = amiga_launch_row_entry(l, row);
  char full[AMIGA_LAUNCH_PATH_MAX + AMIGA_LAUNCH_NAME_MAX];
  size_t n;
  size_t alen = 0;

  if (!e || e->is_dir || !e->is_exe)
    return 0;
  if (strlen(l->path) + strlen(e->name) >= sizeof(full))
    return 0;
  strcpy(full, l->path);
  strcat(full, e->name);
  n = strlen(full);
  if (strchr(full, ' '))
    n += 2;
  if (args) {
    while (*args == ' ')
      args++;
    alen = strlen(args);
    while (alen && args[alen - 1] == ' ')
      alen--;
  }
  if (alen)
    n += 1 + alen;
  if (n + 1 > cap)
    return 0;
  out[0] = 0;
  if (strchr(full, ' ')) {
    strcat(out, "\"");
    strcat(out, full);
    strcat(out, "\"");
  } else {
    strcpy(out, full);
  }
  if (alen) {
    strcat(out, " ");
    strncat(out, args, alen);
  }
  return 1;
}
