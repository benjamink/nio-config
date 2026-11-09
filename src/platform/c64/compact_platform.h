#ifndef C64_COMPACT_PLATFORM_H
#define C64_COMPACT_PLATFORM_H

/* The C64's side of the compact launcher (src/platform/compact/README.md): its screen
 * and keyboard, and the hooks the launcher leaves to the machine. */

#include "c64_input.h"
#include "c64_scr.h"

/* First thing in main(), before the screen exists. */
#define PLATFORM_INIT()

/* The failure screen, after its message: nothing to add. */
#define PLATFORM_FAIL_DETAIL()

/* A key the launcher does not handle itself: F5 runs the demo from the boot disk. */
#define PLATFORM_KEY(k) \
  do { \
    if ((k) == K_DEMO) \
      c64_run_demo(); \
  } while (0)

/* Move a list row's text up or down: the screen code has a call for it. */
#define SCR_COPY_ROW_VARS
#define SCR_COPY_ROW(to, from) scr_copy_row((to), (from))

#endif
