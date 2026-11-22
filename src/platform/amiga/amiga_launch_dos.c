#include "amiga_launch.h"

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <exec/memory.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include <string.h>

/* The AmigaDOS executable (hunk) header: HUNK_HEADER. */
#define HUNK_HEADER_MAGIC 0x000003F3UL

static int has_hunk_header(const char *path)
{
  BPTR f = Open((CONST_STRPTR) path, MODE_OLDFILE);
  UBYTE b[4];
  LONG n;

  if (!f)
    return 0;
  n = Read(f, b, 4);
  Close(f);
  return n == 4 &&
         (((ULONG) b[0] << 24) | ((ULONG) b[1] << 16) | ((ULONG) b[2] << 8) |
          b[3]) == HUNK_HEADER_MAGIC;
}

static LONG last_error;

long amiga_launch_dos_error(void)
{
  return (long) last_error;
}

int amiga_launch_dos_scan(const char *path, amiga_launch_entry_t *out,
                          uint16_t max, uint16_t *count, void *ctx)
{
  struct FileInfoBlock *fib;
  BPTR lock;
  char full[AMIGA_LAUNCH_PATH_MAX + AMIGA_LAUNCH_NAME_MAX];
  size_t base = strlen(path);
  uint16_t n = 0;
  int ok = 0;

  struct Process *me = (struct Process *) FindTask(NULL);
  APTR old_window = me->pr_WindowPtr;

  (void) ctx;
  *count = 0;
  if (base + AMIGA_LAUNCH_NAME_MAX >= sizeof(full))
    return 0;
  fib = (struct FileInfoBlock *) AllocMem(sizeof(*fib), MEMF_PUBLIC | MEMF_CLEAR);
  if (!fib)
    return 0;
  /* No "Please insert volume" requester for a drive that is not there. */
  me->pr_WindowPtr = (APTR) -1;
  lock = Lock((CONST_STRPTR) path, ACCESS_READ);
  last_error = lock ? 0 : IoErr();
  me->pr_WindowPtr = old_window;
  if (lock && Examine(lock, fib)) {
    ok = 1;
    last_error = 0;
    while (n < max && ExNext(lock, fib)) {
      amiga_launch_entry_t *e = &out[n];
      size_t len = strlen((const char *) fib->fib_FileName);

      /* Names that do not fit would not open through the path anyway. */
      if (len == 0 || len >= AMIGA_LAUNCH_NAME_MAX)
        continue;
      memset(e, 0, sizeof(*e));
      memcpy(e->name, fib->fib_FileName, len + 1);
      e->size = (uint32_t) fib->fib_Size;
      if (fib->fib_DirEntryType > 0) {
        e->is_dir = 1;
      } else if (len > 5 && strcmp(e->name + len - 5, ".info") == 0) {
        continue;   /* icon files are never programs */
      } else if (fib->fib_Size >= 24) {
        strcpy(full, path);
        strcat(full, e->name);
        e->is_exe = (uint8_t) has_hunk_header(full);
      }
      n++;
    }
    /* ExNext ends with ERROR_NO_MORE_ENTRIES; anything else is a failure. */
    if (n < max && IoErr() != ERROR_NO_MORE_ENTRIES)
      ok = 0;
  }
  else if (lock)
    last_error = IoErr();
  if (lock)
    UnLock(lock);
  FreeMem(fib, sizeof(*fib));
  *count = n;
  return ok;
}
