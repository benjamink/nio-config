#ifndef AMIGA_CTL_H
#define AMIGA_CTL_H

#include "config_nio.h"
#include "amiga_list.h"

typedef enum {
  AMIGA_PAGE_HOSTS = 0,
  AMIGA_PAGE_BROWSE,
  AMIGA_PAGE_CATALOGUE,
  AMIGA_PAGE_DRIVES,
  AMIGA_PAGE_COUNT
} amiga_page_t;

#define AMIGA_CAT_WINDOW 16
#define AMIGA_CAT_SLOTS 256
#define AMIGA_CMD_MAX 128

/* Runs a Shell command (FMOUNT/FUMOUNT) and returns its return code. */
typedef int (*amiga_exec_fn)(const char *command, void *ctx);

/* Controller shared by the Intuition front end and the SCRIPT= driver.
 * Every operation commits to the FujiNet at once and reports through
 * state->status, like the other config-nio front ends. */
typedef struct {
  config_nio_state_t *state;
  uint8_t page;
  amiga_list_t hosts;
  amiga_list_t entries;
  amiga_list_t catalogue;
  amiga_list_t drives;
  uint8_t browse_host;
  uint8_t browse_open;
  uint8_t kick13;
  const char *fmount;
  const char *fumount;
  amiga_exec_fn exec;
  void *exec_ctx;
  uint16_t cat_base;
  config_nio_slot_t cat[AMIGA_CAT_WINDOW];
  char msg[CONFIG_NIO_STATUS_MAX + 1];
  char cmd[AMIGA_CMD_MAX];
} amiga_ctl_t;

void amiga_ctl_init(amiga_ctl_t *ctl, config_nio_state_t *state,
                    uint8_t kick13, uint8_t rows, amiga_exec_fn exec,
                    void *exec_ctx);
void amiga_ctl_set_tools(amiga_ctl_t *ctl, const char *fmount,
                         const char *fumount);
void amiga_ctl_set_rows(amiga_ctl_t *ctl, uint8_t rows);
void amiga_ctl_set_page(amiga_ctl_t *ctl, uint8_t page);

int amiga_ctl_host_add(amiga_ctl_t *ctl, const char *uri);
int amiga_ctl_host_replace(amiga_ctl_t *ctl, const char *uri);
int amiga_ctl_host_remove(amiga_ctl_t *ctl);
int amiga_ctl_host_move(amiga_ctl_t *ctl, int8_t delta);
int amiga_ctl_set_prefs(amiga_ctl_t *ctl, uint8_t date_format,
                        uint8_t size_format);

int amiga_ctl_browse_open(amiga_ctl_t *ctl);
int amiga_ctl_browse_refresh(amiga_ctl_t *ctl);
int amiga_ctl_browse_activate(amiga_ctl_t *ctl);
int amiga_ctl_browse_parent(amiga_ctl_t *ctl);
int amiga_ctl_browse_select_name(amiga_ctl_t *ctl, const char *name);
int amiga_ctl_browse_uri(amiga_ctl_t *ctl, char *out, uint16_t cap);
int amiga_ctl_browse_assign(amiga_ctl_t *ctl, uint8_t slot,
                            uint8_t readonly);

/* Catalogue rows are read in aligned windows of AMIGA_CAT_WINDOW slots and
 * cached until the next write.  NULL means the read failed (see status). */
const config_nio_slot_t *amiga_ctl_slot(amiga_ctl_t *ctl, uint8_t slot);
int amiga_ctl_slot_set(amiga_ctl_t *ctl, uint8_t slot, const char *uri,
                       uint8_t readonly);
int amiga_ctl_slot_clear(amiga_ctl_t *ctl, uint8_t slot);

#endif
