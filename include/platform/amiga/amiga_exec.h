#ifndef AMIGA_EXEC_H
#define AMIGA_EXEC_H

#include <stdint.h>

/* Runs a Shell command around a FujiNet session restart, capturing its last
 * output line; satisfies amiga_exec_fn. */
int amiga_exec_command(const char *command, char *output, uint16_t cap,
                       void *ctx);

/* Runs `command` with `dir` as the current directory (a drawer or drive
 * root; may be empty) and no console: its output is discarded.  With `async`
 * (Workbench 2.0+) the program is started and this returns at once, so the
 * caller can quit and free its memory; Kickstart 1.3 cannot, and waits.
 * Unlike amiga_exec_command the FujiNet session is left alone.  Returns the
 * command's return code, or 0 once an asynchronous program has started. */
int amiga_exec_run(const char *dir, const char *command, int async);

#endif
