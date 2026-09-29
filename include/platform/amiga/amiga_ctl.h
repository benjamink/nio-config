#ifndef AMIGA_CTL_H
#define AMIGA_CTL_H

#include "config_nio.h"
#include "amiga_list.h"

typedef enum {
  AMIGA_PAGE_HOSTS = 0,
  AMIGA_PAGE_BROWSE,
  AMIGA_PAGE_CATALOGUE,
  AMIGA_PAGE_DRIVES,
  AMIGA_PAGE_MOUNT,   /* drive picker; entered only via mount_begin_* */
  AMIGA_PAGE_HELP,    /* built-in help; entered only via help_open */
  AMIGA_PAGE_COUNT
} amiga_page_t;

#define AMIGA_CAT_WINDOW 16
#define AMIGA_CAT_SLOTS 256
#define AMIGA_CMD_MAX 128

#define AMIGA_CMD_OUT_MAX 80

/* Runs a Shell command (FMOUNT/FUMOUNT) and returns its return code; the
 * command's last non-empty output line is copied to `output`. */
typedef int (*amiga_exec_fn)(const char *command, char *output, uint16_t cap,
                             void *ctx);

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
  /* Two catalogue windows (a visible list spans at most two) and one
   * record per drive; AMIGA_CAT_SLOTS / a clear bit mean "not cached". */
  uint16_t cat_base[2];
  uint8_t cat_victim;
  config_nio_slot_t cat[2][AMIGA_CAT_WINDOW];
  uint8_t drive_cached;
  config_nio_slot_t drive_cat[8];
  char msg[CONFIG_NIO_STATUS_MAX + 1];
  char cmd[AMIGA_CMD_MAX];
  char cmd_out[AMIGA_CMD_OUT_MAX];
  /* Pending mount: an image URI from Browse, or a catalogue slot. */
  char mount_uri[CONFIG_NIO_URI_MAX + 1];
  char mount_name[40];
  int16_t mount_slot;
  uint8_t mount_return;
  uint8_t help_topic;
  uint8_t help_return;
  amiga_list_t help;   /* help lines (topic text) or titles (Contents) */
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

/* Drives are mounted by the FMOUNT/FUMOUNT commands, which own the DOS
 * node lifecycle and persist config-nio/mappings.  Success is judged by the
 * reloaded mapping, not only by the command's return code. */
int amiga_ctl_reload(amiga_ctl_t *ctl);
int amiga_ctl_drive_insert(amiga_ctl_t *ctl, uint8_t unit, uint8_t slot,
                           uint8_t readonly);
int amiga_ctl_drive_eject(amiga_ctl_t *ctl, uint8_t unit);
/* Catalogue record for a mapped drive (one read per drive until the next
 * reload or write); NULL when the drive is unmapped or the read failed. */
const config_nio_slot_t *amiga_ctl_drive_slot(amiga_ctl_t *ctl, uint8_t unit);

/* RO flag to show for a catalogue slot: its stored mode, or 1 when empty. */
uint8_t amiga_ctl_catalogue_readonly(amiga_ctl_t *ctl, uint8_t slot);

/* Mount flow: pick an image (Browse) or slot (Catalogue), then a drive.
 * Commit finds the catalogue slot already holding the image, else the
 * first free one, writes the RO choice to it and runs FMOUNT. */
int amiga_ctl_drive_mounted(amiga_ctl_t *ctl, uint8_t unit);
/* The DOS name to open in a Workbench window (e.g. "DN0:"), or 0 with a
 * status message when the drive is empty. */
int amiga_ctl_drive_window_name(amiga_ctl_t *ctl, uint8_t unit, char *out,
                                uint16_t cap);
uint8_t amiga_ctl_first_empty_drive(amiga_ctl_t *ctl);
int amiga_ctl_mount_begin_browse(amiga_ctl_t *ctl);
int amiga_ctl_mount_begin_slot(amiga_ctl_t *ctl, uint8_t slot);
int amiga_ctl_mount_commit(amiga_ctl_t *ctl, uint8_t unit, uint8_t readonly);
void amiga_ctl_mount_cancel(amiga_ctl_t *ctl);

/* Help page: open a topic (remembering the page to return to), step to the
 * previous/next topic, and close. */
void amiga_ctl_help_open(amiga_ctl_t *ctl, uint8_t topic);
void amiga_ctl_help_step(amiga_ctl_t *ctl, int8_t delta);
void amiga_ctl_help_close(amiga_ctl_t *ctl);

#endif
