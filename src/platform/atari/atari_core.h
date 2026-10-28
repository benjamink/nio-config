#ifndef ATARI_CORE_H
#define ATARI_CORE_H

/* The data the Atari config app works on: hosts, drive map, directory
 * listing, and boot-time mounting.  It reads and writes the same
 * `config-nio` app-store records and slot catalogue as the other front ends
 * (see docs/state-model.md), but compactly enough for a 48K Atari. */

#include <stdint.h>

#define CORE_NS        "config-nio"
#define CORE_MAX_HOSTS 8
#define CORE_URI_MAX   79
#define CORE_NAME_MAX  31
#define CORE_UNITS     8
#define CORE_WINDOW    22

typedef struct {
  uint8_t flags;                /* CORE_ENTRY_* */
  uint32_t size;
  uint32_t mtime;               /* Unix seconds, 0 if unknown */
  char name[CORE_NAME_MAX + 1];
} core_entry_t;

#define CORE_ENTRY_DIR       0x01
#define CORE_ENTRY_TRUNCATED 0x80

extern char hosts[CORE_MAX_HOSTS][CORE_URI_MAX + 1];
extern uint8_t host_count;

/* Drive map: unit -> catalogue slot, with its URI and write protect. */
extern uint8_t drive_on[CORE_UNITS];
extern uint8_t drive_ro[CORE_UNITS];
extern uint8_t drive_slot[CORE_UNITS];
extern char drive_uri[CORE_UNITS][CORE_URI_MAX + 1];

/* One page of a directory listing. */
extern core_entry_t entries[CORE_WINDOW];
extern uint8_t entry_count;
extern uint8_t entry_more;
extern uint16_t entry_start;
extern uint8_t list_stale;   /* the page no longer matches the directory */

/* FN_* result of the last failed app-store read in core_load(). */
extern uint8_t core_error;

/* A scratch block (the directory page buffer) for callers that need a few
 * hundred bytes briefly; the directory page is marked stale. */
void *core_scratch(void);
#define CORE_SCRATCH_SIZE (CORE_WINDOW * 41)

uint8_t core_load(void);
uint8_t core_save_hosts(void);
uint8_t core_save_drives(void);
uint8_t core_put_slot(uint8_t slot, const char *uri, uint8_t readonly);

/* Compose host + path + leaf into a URI; 1 on success. */
uint8_t core_compose(char *out, uint8_t cap, const char *host,
                     const char *path, const char *leaf);

/* List `uri` from entry `start`.  Fills entries[]; 1 on success. */
uint8_t core_list(const char *uri, uint16_t start);
uint8_t core_list_error(void);
/* Full name of entry `index` (a page keeps only CORE_NAME_MAX characters). */
uint8_t core_full_name(const char *uri, uint16_t index, char *out, uint8_t cap);

/* Mount the drive map (D1 is DiskDevice slot 1); D2-D8 not in the map are unmounted.  Returns the
 * number of drives mounted and sets *failed to the first unit that would
 * not mount (0xFF if none), with core_mount_error() explaining it. */
uint8_t core_mount_all(uint8_t *failed);
const char *core_mount_error(void);

#endif
