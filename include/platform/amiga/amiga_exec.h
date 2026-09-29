#ifndef AMIGA_EXEC_H
#define AMIGA_EXEC_H

/* Runs a Shell command with NIL: I/O around a FujiNet session restart;
 * satisfies amiga_exec_fn. */
int amiga_exec_command(const char *command, void *ctx);

#endif
