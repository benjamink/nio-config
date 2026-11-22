#include "amiga_exec.h"
#include "amiga_format.h"
#include "fujinet-nio.h"

#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dostags.h>

#include <string.h>

/* RAM: exists on every Kickstart; T: is not assigned on a stock 1.3. */
#define AMIGA_EXEC_OUT "RAM:config-nio.out"
#define AMIGA_EXEC_TAIL 512

/* Keeps the last AMIGA_EXEC_TAIL-1 bytes of the command's output. */
static void read_tail(char *buf)
{
  BPTR f;
  LONG n;
  LONG len = 0;

  buf[0] = 0;
  f = Open((CONST_STRPTR) AMIGA_EXEC_OUT, MODE_OLDFILE);
  if (!f)
    return;
  for (;;) {
    if (len == AMIGA_EXEC_TAIL - 1) {
      memmove(buf, buf + AMIGA_EXEC_TAIL / 2, AMIGA_EXEC_TAIL / 2 - 1);
      len = AMIGA_EXEC_TAIL / 2 - 1;
    }
    n = Read(f, buf + len, AMIGA_EXEC_TAIL - 1 - len);
    if (n <= 0)
      break;
    len += n;
  }
  buf[len] = 0;
  Close(f);
}

int amiga_exec_command(const char *command, char *output, uint16_t cap,
                       void *ctx)
{
  static char tail[AMIGA_EXEC_TAIL];
  BPTR out;
  LONG rc;

  (void) ctx;
  /* Mirror FMOUNTRESTORE: release this process's FujiNet session while a
   * child command opens its own, then reconnect so the caller can reload. */
  fn_shutdown();
  out = Open((CONST_STRPTR) AMIGA_EXEC_OUT, MODE_NEWFILE);
  if (!out)
    out = Open((CONST_STRPTR) "NIL:", MODE_NEWFILE);
  if (((struct Library *) DOSBase)->lib_Version < 36) {
    /* KS1.3 Execute() reports only whether the command started; amiga_ctl
     * verifies the resulting mapping rather than trusting this value. */
    rc = Execute((CONST_STRPTR) command, 0, out) ? 0 : 20;
  } else {
    BPTR in = Open((CONST_STRPTR) "NIL:", MODE_OLDFILE);

    rc = SystemTags((CONST_STRPTR) command, SYS_Input, in, SYS_Output, out,
                    TAG_DONE);
    if (in)
      Close(in);
  }
  if (out)
    Close(out);
  read_tail(tail);
  amiga_last_line(output, cap, tail);
  (void) DeleteFile((CONST_STRPTR) AMIGA_EXEC_OUT);
  if (fn_init() != FN_OK)
    return rc ? (int) rc : 20;
  return (int) rc;
}

int amiga_exec_run(const char *dir, const char *command, int async)
{
  BPTR lock = 0;
  BPTR old = 0;
  BPTR in;
  BPTR out;
  LONG rc;
  int started_async = 0;

  if (dir && dir[0]) {
    lock = Lock((CONST_STRPTR) dir, SHARED_LOCK);
    if (lock)
      old = CurrentDir(lock);
  }
  in = Open((CONST_STRPTR) "NIL:", MODE_OLDFILE);
  out = Open((CONST_STRPTR) "NIL:", MODE_NEWFILE);
  if (((struct Library *) DOSBase)->lib_Version < 36) {
    /* KS1.3 has no asynchronous Execute(); it waits for the program. */
    rc = Execute((CONST_STRPTR) command, 0, out) ? 0 : 20;
  } else if (async) {
    /* A started asynchronous program owns (and closes) its streams, and
     * the copy of the directory lock it is given (we unlock ours). */
    BPTR dup = lock ? DupLock(lock) : 0;

    rc = SystemTags((CONST_STRPTR) command, SYS_Input, in, SYS_Output, out,
                    SYS_Asynch, TRUE, NP_CurrentDir, dup, TAG_DONE);
    started_async = rc != -1;
    if (!started_async && dup)
      UnLock(dup);
    rc = started_async ? 0 : 20;
  } else {
    rc = SystemTags((CONST_STRPTR) command, SYS_Input, in, SYS_Output, out,
                    TAG_DONE);
  }
  if (!started_async) {
    if (in)
      Close(in);
    if (out)
      Close(out);
  }
  if (lock) {
    (void) CurrentDir(old);
    UnLock(lock);
  }
  return (int) rc;
}
