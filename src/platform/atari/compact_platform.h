#ifndef ATARI_COMPACT_PLATFORM_H
#define ATARI_COMPACT_PLATFORM_H

/* The Atari's side of the compact launcher (src/platform/compact/README.md): its screen
 * and keyboard, and the hooks the launcher leaves to the machine. */

#include "atari_input.h"
#include "atari_scr.h"

/* The shift-lock flag: clear means lower case, which URIs need. */
#define SHFLOK (*(volatile uint8_t *) 0x02BE)

/* First thing in main(), before the screen exists. */
#define PLATFORM_INIT() (SHFLOK = 0)

/* The failure screen, after its message: the last SIO status. */
#define PLATFORM_FAIL_DETAIL() \
  do { \
    scr_text(ROW_STATUS, 34, "sio", 3); \
    scr_uint(ROW_STATUS, 36, fn_atari_last_sio_status(), 3); \
  } while (0)

/* A key the launcher does not handle itself: the Atari has no keys of its own. */
#define PLATFORM_KEY(k)

/* Move a list row's text up or down. The screen is plain memory addressed through the
 * row tables, so this is a copy, done in place with the caller's two pointers. */
#define SCR_COPY_ROW_VARS \
  uint8_t *dst; \
  uint8_t *src;
#define SCR_COPY_ROW(to, from) \
      dst = (uint8_t *) (scr_rowlo[to] | (scr_rowhi[to] << 8)); \
      src = (uint8_t *) (scr_rowlo[from] | (scr_rowhi[from] << 8)); \
      memcpy(dst, src, SCR_COLS)

#endif
