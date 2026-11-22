#ifndef AMIGA_LAUNCH_H
#define AMIGA_LAUNCH_H

#include <stdint.h>

#include "amiga_list.h"

/* Launch window logic: list the programs on a mounted drive and build the
 * command line that runs one.  Pure C over an injected directory scanner, so
 * the host tests need no AmigaDOS; amiga_launch_dos.c supplies the real one. */

#define AMIGA_LAUNCH_NAME_MAX 32       /* AmigaDOS names are at most 30 */
#define AMIGA_LAUNCH_MAX 128           /* entries kept from one folder */
#define AMIGA_LAUNCH_PATH_MAX 160
#define AMIGA_LAUNCH_ARGS_MAX 80
#define AMIGA_LAUNCH_CMD_MAX (AMIGA_LAUNCH_PATH_MAX + AMIGA_LAUNCH_ARGS_MAX + 4)

typedef struct {
  char name[AMIGA_LAUNCH_NAME_MAX];
  uint8_t is_dir;
  uint8_t is_exe;      /* starts with the AmigaDOS executable (hunk) header */
  uint32_t size;
} amiga_launch_entry_t;

/* Reads the folder `path` (a volume root such as "DN0:" or a drawer ending
 * in '/'): at most `max` entries into `out`, the count in *count.  Returns 0
 * when the folder cannot be read. */
typedef int (*amiga_launch_scan_fn)(const char *path, amiga_launch_entry_t *out,
                                    uint16_t max, uint16_t *count, void *ctx);

typedef struct {
  amiga_launch_scan_fn scan;
  void *scan_ctx;
  char path[AMIGA_LAUNCH_PATH_MAX];   /* the folder being shown */
  uint8_t depth;                       /* drawers below the drive root */
  amiga_launch_entry_t entries[AMIGA_LAUNCH_MAX];   /* drawers, then programs */
  uint16_t count;
  uint8_t truncated;                   /* the folder had more than we keep */
  const char *error;                   /* why the last operation failed */
  amiga_list_t list;
} amiga_launch_t;

enum {
  AMIGA_LAUNCH_FAILED = 0,
  AMIGA_LAUNCH_ENTERED,    /* a drawer or ".." was opened */
  AMIGA_LAUNCH_PROGRAM     /* the row is a program: run it */
};

/* The real scanner (amiga_launch_dos.c): AmigaDOS Lock/ExNext, with a program
 * recognised by its executable header.  Not part of the host tests. */
int amiga_launch_dos_scan(const char *path, amiga_launch_entry_t *out,
                          uint16_t max, uint16_t *count, void *ctx);

/* AmigaDOS error behind the last failed amiga_launch_dos_scan (0 if none). */
long amiga_launch_dos_error(void);

void amiga_launch_init(amiga_launch_t *l, uint8_t rows,
                       amiga_launch_scan_fn scan, void *scan_ctx);
/* Opens a drive's root, e.g. "DN0:".  Returns 0 (with l->error) on failure. */
int amiga_launch_open(amiga_launch_t *l, const char *root);

/* Rows: ".." first inside a drawer, then the entries. */
uint16_t amiga_launch_rows(const amiga_launch_t *l);
int amiga_launch_row_is_parent(const amiga_launch_t *l, uint16_t row);
/* NULL for ".." and past the end. */
const amiga_launch_entry_t *amiga_launch_row_entry(const amiga_launch_t *l,
                                                   uint16_t row);

/* Return / double-click on a row. */
int amiga_launch_activate(amiga_launch_t *l, uint16_t row);
int amiga_launch_parent(amiga_launch_t *l);

/* Full command for the program on `row` with `args` (may be empty): the path
 * is quoted when it holds a space.  Returns 0 when the row is not a program
 * or the command does not fit. */
int amiga_launch_command(const amiga_launch_t *l, uint16_t row,
                         const char *args, char *out, uint16_t cap);

#endif
