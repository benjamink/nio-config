#include "amiga_exec.h"
#include "fujinet-nio.h"

#include <dos/dos.h>
#include <proto/dos.h>
#ifndef __KICK13__
#include <dos/dostags.h>
#endif

int amiga_exec_command(const char *command, void *ctx)
{
  BPTR out;
  LONG rc;

  (void) ctx;
  /* Mirror FMOUNTRESTORE: release this process's FujiNet session while a
   * child command opens its own, then reconnect so the caller can reload. */
  fn_shutdown();
  out = Open((CONST_STRPTR) "NIL:", MODE_NEWFILE);
#ifdef __KICK13__
  /* KS1.3 Execute() reports only whether the command started; amiga_ctl
   * verifies the resulting mapping rather than trusting this value. */
  rc = Execute((CONST_STRPTR) command, 0, out) ? 0 : 20;
#else
  {
    BPTR in = Open((CONST_STRPTR) "NIL:", MODE_OLDFILE);

    rc = SystemTags((CONST_STRPTR) command, SYS_Input, in, SYS_Output, out,
                    TAG_DONE);
    if (in)
      Close(in);
  }
#endif
  if (out)
    Close(out);
  if (fn_init() != FN_OK)
    return rc ? (int) rc : 20;
  return (int) rc;
}
