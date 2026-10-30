#ifndef FNSVC_H
#define FNSVC_H

#include <stdint.h>

#define FNSVC_MAX_URI   255
#define FNSVC_MAX_PATH  127

enum {
  FNSVC_STATUS_OK = 0
};

enum {
  FNSVC_ERR_NONE = 0,
  FNSVC_ERR_INVALID_ARG,
  FNSVC_ERR_REQUEST_TOO_LARGE,
  FNSVC_ERR_TRANSPORT,
  FNSVC_ERR_STATUS,
  FNSVC_ERR_BAD_VERSION,
  FNSVC_ERR_SHORT_RESPONSE,
  FNSVC_ERR_ENTRIES_BOUNDS,
  FNSVC_ERR_ENTRY_BOUNDS,
  FNSVC_ERR_UNSUPPORTED  /* firmware has no ListPages */
};

typedef void (*fnsvc_list_cb)(uint8_t is_dir,
                              const char *name,
                              uint32_t size,
                              uint32_t mtime,
                              void *ctx);

int fnsvc_list_directory(const char *uri, fnsvc_list_cb cb, void *ctx);
/* Paged listing (ListPages): the device sorts the folder once, directories
 * first, and hands out pages of FNSVC_PAGE_SIZE entries.  The page size must
 * stay fixed while one folder is browsed, because page numbers depend on it.
 * first_entry must be a multiple of FNSVC_PAGE_SIZE.  One call delivers whole
 * pages (as many as fit FNSVC_PAGE_BUDGET bytes) through cb, sets *total to the
 * folder's entry count and *delivered to the entries passed to cb.  refresh
 * makes the device re-read the folder.  Returns 0 on failure; fnsvc_last_error()
 * is FNSVC_ERR_UNSUPPORTED on firmware without ListPages, so callers can fall
 * back to fnsvc_list_directory(). */
#define FNSVC_PAGE_SIZE 4
int fnsvc_list_pages(const char *uri, uint32_t first_entry, uint8_t refresh,
                     fnsvc_list_cb cb, void *ctx, uint32_t *total,
                     uint16_t *delivered);
int fnsvc_resolve_path(const char *base_uri, const char *arg,
                       char *resolved_uri, uint16_t resolved_cap,
                       char *display_path, uint16_t display_cap,
                       uint8_t *flags_out);
/* Mount `uri` in active disk slot `slot`, with its image type detected by
 * NIO. sector_size_hint is the sector size this machine uses for headerless
 * raw media: NIO applies it only when the image's content does not identify
 * its geometry and its extension is ambiguous (for example .dsk, which is
 * 256-byte sectors on an Apple II and 512 on a Mac). Pass 0 for no hint. See
 * "Detection policy" in fujinet-nio's docs/disk_device_protocol.md. */
int fnsvc_disk_mount(uint8_t slot, const char *uri, uint8_t readonly,
                     uint16_t sector_size_hint);
int fnsvc_disk_unmount(uint8_t slot);
/* FN_DISK_ERR_* (fujinet-nio.h) for the most recent fnsvc_disk_*() call:
 * FN_DISK_ERR_NONE after success, the exact reason after a failure NIO
 * explained, otherwise FN_DISK_ERR_UNREPORTED. */
uint8_t fnsvc_disk_last_error(void);
int fnsvc_disk_restore_boot(uint8_t slot);
uint8_t fnsvc_last_error(void);
uint8_t fnsvc_last_status(void);
uint8_t fnsvc_last_raw_error(void);
uint16_t fnsvc_last_response_len(void);

#endif
