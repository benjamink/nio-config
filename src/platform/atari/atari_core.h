#ifndef ATARI_CORE_H
#define ATARI_CORE_H

/* The data the Atari config app works on: hosts, drive map, directory
 * listing, and boot-time mounting.  It reads and writes the same
 * `config-nio` app-store records and slot catalogue as the other front ends
 * (see docs/state-model.md), but compactly enough for a 48K Atari. */

#include <stdint.h>

#define CORE_NS        "config-nio"
#define CORE_MAX_HOSTS 24
#define CORE_HOST_POOL 1000  /* bytes for all saved hosts together */
#define CORE_URI_MAX   196   /* a whole URI: host + path + name */
#define CORE_HOST_MAX  100   /* one saved host */
#define CORE_NAME_MAX  31
#define CORE_UNITS     8
#define CORE_PAGE_ROWS  18    /* entries on a screen page */
#ifdef CONFIG_NIO_BANKED
#define CORE_PROTO_PAGE 2     /* 2 full 220-byte names fit in one reply */
#define CORE_PCACHE     8192  /* upper half of the Setup bank */
#else
#define CORE_PROTO_PAGE 9
#define CORE_PCACHE     800
#endif

typedef struct {
  uint8_t flags;                /* CORE_ENTRY_* */
  uint32_t size;
  uint32_t mtime;               /* Unix seconds, 0 if unknown */
  uint16_t disp;                /* size as shown: kilobytes, or megabytes with CORE_ENTRY_MB */
  char name[CORE_NAME_MAX + 1];
} core_entry_t;

#define CORE_ENTRY_DIR       0x01
#define CORE_ENTRY_MB        0x02   /* disp is megabytes */
#define CORE_ENTRY_TRUNCATED 0x80

extern char *host_ptr[CORE_MAX_HOSTS + 1];   /* host_ptr[i]: saved host i */
extern uint8_t host_count;

/* Drive map: unit -> catalogue slot, with its URI and write protect. */
extern uint8_t drive_on[CORE_UNITS];
extern uint8_t drive_ro[CORE_UNITS];
extern uint8_t drive_slot[CORE_UNITS];
/* The end of each drive's URI (the part that shows: the name); the whole URI is
 * kept in the slot catalogue and read back when mounting. */
#define CORE_DRIVE_URI 80
extern char drive_uri[CORE_UNITS][CORE_DRIVE_URI + 1];
/* Set drive_uri[unit] from a URI of any length: its last CORE_DRIVE_URI chars. */
void core_drive_tail(uint8_t unit, const char *uri, uint16_t len);

/* The screen page being shown: entries[0..entry_count-1].  dir_total is the
 * number of entries in the directory (0xFFFFFFFF until the first reply). */
extern core_entry_t entries[CORE_PAGE_ROWS];
extern uint8_t entry_count;
extern uint32_t dir_total;
extern uint8_t list_stale;   /* entries[] was borrowed: rebuild it with core_dir_page */

/* FN_* result of the last failed app-store read in core_load(). */
extern uint8_t core_error;

/* A scratch block (the directory page buffer) for callers that need a few
 * hundred bytes briefly; the directory page is marked stale. */
void *core_scratch(void);
#define CORE_SCRATCH_SIZE (CORE_PAGE_ROWS * 43)

uint8_t core_load(void);
uint8_t core_save_hosts(void);
/* Replace host i with s, or append if i == host_count; 0 if the pool is full. */
uint8_t core_host_set(uint8_t i, const char *s);
void core_host_delete(uint8_t i);
uint8_t core_save_drives(void);
uint8_t core_put_slot(uint8_t slot, const char *uri, uint8_t readonly);

/* Compose host + path + leaf into a URI; 1 on success. */
extern char *compose_out;
extern const char *compose_host, *compose_path, *compose_leaf;
extern uint8_t compose_cap;
#pragma zpsym("compose_out")
#pragma zpsym("compose_host")
#pragma zpsym("compose_path")
#pragma zpsym("compose_leaf")
#pragma zpsym("compose_cap")
uint8_t compose_run(void);
#define core_compose(out, cap, host, path, leaf) \
  (compose_out = (out), compose_cap = (cap), compose_host = (host), \
   compose_path = (path), compose_leaf = (leaf), compose_run())

/* Forget the cached pages (a different directory, or a refresh). */
void core_dir_reset(void);
/* 1 if every protocol page of screen page `screen_page` is already held. */
uint8_t core_dir_have(uint16_t screen_page);
/* Make entries[] the screen page `screen_page` (CORE_PAGE_ROWS entries) of the
 * directory `uri`, asking the device only for pages not already held.  refresh
 * (FN_LIST_REFRESH-style) makes the device re-read the directory on its next
 * reply.  1 on success; on failure entries[] is unchanged. */
uint8_t core_dir_page(const char *uri, const char *filter, uint16_t screen_page, uint8_t refresh);
uint8_t core_list_error(void);
/* Full name of entry `index` (a page keeps only CORE_NAME_MAX characters). */
uint8_t core_full_name(const char *uri, const char *filter, uint16_t index, char *out, uint8_t cap);

/* Mount the drive map (D1 is DiskDevice slot 1); D2-D8 not in the map are unmounted.  Returns the
 * number of drives mounted and sets *failed to the first unit that would
 * not mount (0xFF if none), with core_mount_error() explaining it. */
uint8_t core_mount_all(uint8_t *failed);
const char *core_mount_error(void);

#endif
