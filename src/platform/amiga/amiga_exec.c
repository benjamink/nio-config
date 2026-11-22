#include "amiga_exec.h"
#include "amiga_format.h"
#include "fujinet-nio.h"

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <exec/memory.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dostags.h>

#include <string.h>

/* T: is not assigned on a stock 1.3.  RAM: needs L:Ram-Handler there, so a
 * bare (no Workbench) 1.3 boot disk has none: the capture then goes to NIL:. */
#define AMIGA_EXEC_OUT "RAM:config-nio.out"
#define AMIGA_EXEC_TAIL 512

/* Open without DOS "insert volume" requesters (a bare 1.3 boot has no RAM:). */
static BPTR open_quiet(const char *path, LONG mode)
{
  struct Process *self = (struct Process *) FindTask(NULL);
  APTR old = self->pr_WindowPtr;
  BPTR f;

  self->pr_WindowPtr = (APTR) -1;
  f = Open((CONST_STRPTR) path, mode);
  self->pr_WindowPtr = old;
  return f;
}

#define EXEC_BADDR(b) ((APTR) ((ULONG) (b) << 2))
#define EXEC_MKBADDR(p) ((BPTR) ((ULONG) (p) >> 2))
#define EXEC_NAME_MAX 64
#define EXEC_ARGS_MAX 256

/* LoadSeg a command the way the Shell finds it: a path as given, otherwise
 * the current directory, the CLI's command path, then C:. */
static BPTR load_command(const char *name)
{
  struct Process *self = (struct Process *) FindTask(NULL);
  struct CommandLineInterface *cli = EXEC_BADDR(self->pr_CLI);
  char cpath[EXEC_NAME_MAX + 3];
  BPTR seg;
  BPTR *entry;
  APTR oldwin = self->pr_WindowPtr;

  self->pr_WindowPtr = (APTR) -1;
  seg = LoadSeg((CONST_STRPTR) name);
  if (!seg && !strchr(name, ':') && !strchr(name, '/')) {
    for (entry = cli ? EXEC_BADDR(cli->cli_CommandDir) : NULL; !seg && entry;
         entry = EXEC_BADDR(entry[0])) {
      BPTR old = CurrentDir(entry[1]);
      seg = LoadSeg((CONST_STRPTR) name);
      (void) CurrentDir(old);
    }
    if (!seg) {
      strcpy(cpath, "C:");
      strcat(cpath, name);
      seg = LoadSeg((CONST_STRPTR) cpath);
    }
  }
  self->pr_WindowPtr = oldwin;
  return seg;
}

/* Enter a loaded command as the 1.3 Shell does: the argument line (ending
 * in a newline) in A0, its length in D0; the return code comes back in D0. */
static LONG call_segment(BPTR seg, const char *args, LONG len)
{
  register LONG d0 __asm("d0") = len;
  register const char *a0 __asm("a0") = args;
  register APTR a1 __asm("a1") = (APTR) ((ULONG) EXEC_BADDR(seg) + 4);

  __asm volatile("movem.l d2-d7/a2-a6,-(sp)\n\t"
                 "jsr (a1)\n\t"
                 "movem.l (sp)+,d2-d7/a2-a6"
                 : "+r"(d0), "+r"(a0), "+r"(a1)
                 :
                 : "d1", "memory", "cc");
  return d0;
}

/*
 * Kickstart 1.3 Execute() needs C:Run, a Workbench file, so it cannot start
 * anything from a boot disk without one.  Run the command inside this
 * process instead, as the Shell runs a command: only possible from a CLI
 * (a program started without one waits for a Workbench message).  Returns
 * the command's return code, or -1 when it could not be loaded.
 */
static LONG run_in_process(const char *command, BPTR in, BPTR out)
{
  static LONG name_buf[(EXEC_NAME_MAX + 4) / 4 + 1];
  static char args[EXEC_ARGS_MAX + 2];
  struct Process *self = (struct Process *) FindTask(NULL);
  struct CommandLineInterface *cli = EXEC_BADDR(self->pr_CLI);
  UBYTE *bname = (UBYTE *) name_buf;
  const char *p = command;
  BPTR seg, old_in, old_out, old_module;
  BSTR old_name;
  size_t n = 0, alen;
  LONG rc;

  if (!cli)
    return -1;
  while (*p == ' ')
    p++;
  while (p[n] && p[n] != ' ')
    n++;
  if (n == 0 || n > EXEC_NAME_MAX)
    return -1;
  bname[0] = (UBYTE) n;
  memcpy(bname + 1, p, n);
  bname[1 + n] = 0;
  p += n;
  while (*p == ' ')
    p++;
  alen = strlen(p);
  if (alen > EXEC_ARGS_MAX)
    return -1;
  memcpy(args, p, alen);
  args[alen++] = '\n';
  args[alen] = 0;

  seg = load_command((const char *) bname + 1);
  if (!seg)
    return -1;
  old_in = self->pr_CIS;
  old_out = self->pr_COS;
  old_name = cli->cli_CommandName;
  old_module = cli->cli_Module;
  if (in)
    self->pr_CIS = in;
  if (out)
    self->pr_COS = out;
  cli->cli_CommandName = EXEC_MKBADDR(bname);
  cli->cli_Module = seg;
  rc = call_segment(seg, args, (LONG) alen);
  cli->cli_Module = old_module;
  cli->cli_CommandName = old_name;
  self->pr_CIS = old_in;
  self->pr_COS = old_out;
  UnLoadSeg(seg);
  return rc;
}

/* KS1.3: Execute() where it works (C:Run present), else run in-process.
 * Execute() reports only whether the command started; callers verify. */
static LONG run13(const char *command, BPTR in, BPTR out)
{
  LONG rc;

  if (Execute((CONST_STRPTR) command, 0, out))
    return 0;
  rc = run_in_process(command, in, out);
  return rc < 0 ? 20 : rc;
}

/* Keeps the last AMIGA_EXEC_TAIL-1 bytes of the command's output. */
static void read_tail(char *buf)
{
  BPTR f;
  LONG n;
  LONG len = 0;

  buf[0] = 0;
  f = open_quiet(AMIGA_EXEC_OUT, MODE_OLDFILE);
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
  out = open_quiet(AMIGA_EXEC_OUT, MODE_NEWFILE);
  if (!out)
    out = Open((CONST_STRPTR) "NIL:", MODE_NEWFILE);
  if (((struct Library *) DOSBase)->lib_Version < 36) {
    /* amiga_ctl verifies the resulting mapping rather than trusting rc. */
    BPTR in = Open((CONST_STRPTR) "NIL:", MODE_OLDFILE);

    rc = run13(command, in, out);
    if (in)
      Close(in);
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
  {
    struct Process *self = (struct Process *) FindTask(NULL);
    APTR old = self->pr_WindowPtr;

    self->pr_WindowPtr = (APTR) -1;
    (void) DeleteFile((CONST_STRPTR) AMIGA_EXEC_OUT);
    self->pr_WindowPtr = old;
  }
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
    rc = run13(command, in, out);
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
