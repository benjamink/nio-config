#ifndef AMIGA_EXEC_H
#define AMIGA_EXEC_H

#include <stdint.h>

/* Runs a Shell command around a FujiNet session restart, capturing its last
 * output line; satisfies amiga_exec_fn. */
int amiga_exec_command(const char *command, char *output, uint16_t cap,
                       void *ctx);

#endif
