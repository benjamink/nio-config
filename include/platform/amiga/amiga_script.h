#ifndef AMIGA_SCRIPT_H
#define AMIGA_SCRIPT_H

#include "amiga_ctl.h"

enum {
  AMIGA_SCRIPT_ERR = 0,
  AMIGA_SCRIPT_OK = 1,
  AMIGA_SCRIPT_QUIT = 2,
  AMIGA_SCRIPT_WAIT = 3
};

typedef void (*amiga_script_out_fn)(const char *line, void *ctx);

/* Runs one SCRIPT= command line against the controller (the same calls the
 * gadgets make) and writes its transcript lines through `out`. */
int amiga_script_line(amiga_ctl_t *ctl, const char *line,
                      amiga_script_out_fn out, void *out_ctx,
                      uint16_t *wait_ticks);

#endif
