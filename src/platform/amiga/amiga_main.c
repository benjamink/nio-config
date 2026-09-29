#include "config_nio.h"
#include "fujinet-nio.h"
#include "amiga_ctl.h"
#include "amiga_exec.h"
#include "amiga_gui.h"
#include "amiga_options.h"

#include <exec/types.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>

#include <stdio.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *IconBase;

static config_nio_state_t state;
static amiga_ctl_t ctl;
static amiga_options_t options;

#ifdef __KICK13__
#define AMIGA_KICK13 1
#else
#define AMIGA_KICK13 0
#endif

void config_nio_fatal_message(const char *message)
{
  amiga_gui_fatal(message);
}

static void read_tooltypes(struct WBStartup *wb)
{
  struct WBArg *arg;
  struct DiskObject *dobj;
  BPTR old;
  char **tt;

  if (!IconBase || !wb || wb->sm_NumArgs < 1)
    return;
  arg = &wb->sm_ArgList[0];
  old = CurrentDir(arg->wa_Lock);
  dobj = GetDiskObject((CONST_STRPTR) arg->wa_Name);
  if (dobj) {
    /* Workbench convention: unknown or malformed ToolTypes are ignored. */
    for (tt = (char **) dobj->do_ToolTypes; tt && *tt; tt++)
      (void) amiga_options_parse(&options, *tt);
    FreeDiskObject(dobj);
  }
  (void) CurrentDir(old);
}

int main(int argc, char **argv)
{
  int i;
  int rc = 20;

  IntuitionBase = (struct IntuitionBase *)
    OpenLibrary((CONST_STRPTR) "intuition.library", 33);
  GfxBase = (struct GfxBase *)
    OpenLibrary((CONST_STRPTR) "graphics.library", 33);
  IconBase = OpenLibrary((CONST_STRPTR) "icon.library", 33);
  if (!IntuitionBase || !GfxBase)
    goto out;

  amiga_options_defaults(&options);
  if (argc == 0) {
    /* Both libnix and clib2 pass the WBStartup message as argv. */
    read_tooltypes((struct WBStartup *) argv);
  } else {
    for (i = 1; i < argc; i++) {
      if (amiga_options_parse(&options, argv[i]) != 1) {
        puts("Usage: config-nio [FMOUNT=path] [FUMOUNT=path] "
             "[SCRIPT=file] [RESULT=file]");
        rc = 10;
        goto out;
      }
    }
  }
  amiga_options_finish(&options);

  if (fn_init() != FN_OK) {
    amiga_gui_fatal("FujiNet init failed.\nIs fujinet-nio.device installed?");
    goto out;
  }
  if (!fn_is_ready()) {
    amiga_gui_fatal("FujiNet is not ready.");
    goto shutdown;
  }
  if (!config_nio_load(&state)) {
    amiga_gui_fatal("Unable to load config-nio state.");
    goto shutdown;
  }
  amiga_ctl_init(&ctl, &state, AMIGA_KICK13, 8, amiga_exec_command, NULL);
  amiga_ctl_set_tools(&ctl, options.fmount, options.fumount);
  rc = amiga_gui_run(&ctl, &options);

shutdown:
  fn_shutdown();
out:
  if (IconBase)
    CloseLibrary(IconBase);
  if (GfxBase)
    CloseLibrary((struct Library *) GfxBase);
  if (IntuitionBase)
    CloseLibrary((struct Library *) IntuitionBase);
  return rc;
}
