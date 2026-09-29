#include "amiga_ctl.h"
#include "amiga_drives.h"

#include <stdio.h>
#include <string.h>

static void status(amiga_ctl_t *ctl, const char *msg)
{
  config_nio_set_status(ctl->state, msg);
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
  ctl->cat_base = AMIGA_CAT_SLOTS;
  amiga_list_init(&ctl->hosts, rows);
  amiga_list_init(&ctl->entries, rows);
  amiga_list_init(&ctl->catalogue, rows);
  amiga_list_init(&ctl->drives, rows);
  amiga_list_set_count(&ctl->hosts, state->host_count);
  amiga_list_set_count(&ctl->catalogue, AMIGA_CAT_SLOTS);
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
}

void amiga_ctl_set_page(amiga_ctl_t *ctl, uint8_t page)
{
  if (page < AMIGA_PAGE_COUNT)
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
