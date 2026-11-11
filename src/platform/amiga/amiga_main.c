#include "config_nio.h"
#include "fnsvc.h"
#include "fujinet-nio.h"
#include "amiga_ctl.h"
#include "amiga_exec.h"
#include "amiga_fmt.h"
#include "amiga_gui.h"
#include "amiga_net.h"
#include "amiga_options.h"
#include "amiga_splash.h"

#include <exec/types.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>

#include <stdio.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *IconBase;

static config_nio_state_t state;
static amiga_ctl_t ctl;
static amiga_options_t options;

/* A drive is mounted when its DOS device (DN0 ... DN7) exists: FMOUNT
 * creates it and FUMOUNT removes it, and a reboot starts without it.
 * WB1.3 uses permanent MountList entries, so it has no such probe. */
static int drive_present(uint8_t unit, void *ctx)
{
  char name[4];
  struct DosList *list;
  int found = 0;

  (void) ctx;
  name[0] = 'D';
  name[1] = 'N';
  name[2] = (char) ('0' + unit);
  name[3] = 0;
  list = LockDosList(LDF_DEVICES | LDF_READ);
  if (list) {
    found = FindDosEntry(list, (CONST_STRPTR) name, LDF_DEVICES) != NULL;
    UnLockDosList(LDF_DEVICES | LDF_READ);
  }
  return found;
}

/* Density and filesystem of an image, from the firmware's Inspect. */
static int inspect_media(const char *uri, amiga_media_t *media, void *ctx)
{
  fnsvc_disk_inspection_t in;

  (void) ctx;
  if (!fnsvc_disk_inspect(uri, &in))
    return 0;
  amiga_media_classify(media, in.sector_size, in.sector_count, in.boot,
                       in.boot_len);
  return media->known;
}

/* WB3.1's FastFileSystem cannot exit (no ACTION_DIE), so a started DNx:
 * handler stays fixed to its geometry and filesystem; FMOUNT refuses another
 * type on it.  Other versions retire the handler, so they pin nothing
 * (amiga_handler_pins_media).
 * The node's DosEnvec holds the type: more than 11 blocks per track is HD,
 * and the DosType's low bit is FFS. */
static int handler_pinned(uint8_t unit, amiga_media_t *pinned, void *ctx)
{
  char name[4];
  struct DosList *list;
  struct DosList *entry;
  int found = 0;

  (void) ctx;
  if (!amiga_handler_pins_media(((struct Library *) DOSBase)->lib_Version))
    return 0;
  name[0] = 'D';
  name[1] = 'N';
  name[2] = (char) ('0' + unit);
  name[3] = 0;
  list = LockDosList(LDF_DEVICES | LDF_READ);
  if (list) {
    entry = FindDosEntry(list, (CONST_STRPTR) name, LDF_DEVICES);
    if (entry && entry->dol_Task &&
        entry->dol_misc.dol_handler.dol_Startup) {
      struct FileSysStartupMsg *fssm = (struct FileSysStartupMsg *)
        BADDR(entry->dol_misc.dol_handler.dol_Startup);
      struct DosEnvec *env = fssm && fssm->fssm_Environ
        ? (struct DosEnvec *) BADDR(fssm->fssm_Environ) : NULL;

      if (env) {
        pinned->known = 1;
        pinned->hd = (uint8_t) (env->de_BlocksPerTrack > 11);
        pinned->ofs = (uint8_t) ((env->de_DosType & 1) == 0);
        found = 1;
      }
    }
    UnLockDosList(LDF_DEVICES | LDF_READ);
  }
  return found;
}

/* FMOUNT with no arguments only prints its usage, which tells the two builds
 * apart.  An unreadable answer (FMOUNT missing, a custom path with spaces) is
 * left for the first real mount to report. */
static int tools_match(uint8_t kick13)
{
  char out[AMIGA_CMD_OUT_MAX];
  char msg[320];
  const char *c;

  for (c = options.fmount; *c; c++) {
    if (*c == ' ')
      return 1;
  }
  out[0] = 0;
  (void) amiga_exec_command(options.fmount, out, sizeof(out), NULL);
  if (!amiga_tools_mismatch(kick13, amiga_fmount_flavour(out), msg,
                            sizeof(msg)))
    return 1;
  amiga_gui_fatal(msg);
  return 0;
}

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
  /* Kickstart 1.x has no dynamic DOS nodes: it uses the fixed DN/HN/DO/HO
   * MountList endpoints, and 2.0+ the eight interchangeable DN0:-DN7:. */
  uint8_t kick13 = (uint8_t) (((struct Library *) SysBase)->lib_Version < 36);

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
             "[SPLASH=ON|OFF] [SCRIPT=file] [RESULT=file]");
        rc = 10;
        goto out;
      }
    }
  }
  amiga_options_finish(&options);

  /* A scripted run has nobody to look at it. */
  if (!options.script[0] && !options.no_splash)
    amiga_splash_open("Connecting to FujiNet...");
  if (fn_init() != FN_OK) {
    amiga_gui_fatal("FujiNet init failed.\nIs fujinet-nio.device installed?");
    goto out;
  }
  if (!fn_is_ready()) {
    amiga_gui_fatal("FujiNet is not ready.");
    goto shutdown;
  }
  amiga_splash_text("Loading hosts...");
  if (!config_nio_load(&state)) {
    char msg[96];
    uint8_t code = 0;
    const char *step = config_nio_load_error(&code);

    amiga_sprintf(msg, "Unable to load config-nio state.\nReading the %s failed: FujiNet error %u.",
                  step, (unsigned) code);
    amiga_gui_fatal(msg);
    goto shutdown;
  }
  if (!tools_match(kick13))
    goto shutdown;
  amiga_ctl_init(&ctl, &state, kick13, 8, amiga_exec_command, NULL);
  amiga_ctl_set_tools(&ctl, options.fmount, options.fumount);
  if (((struct Library *) DOSBase)->lib_Version >= 36)
    amiga_ctl_set_probe(&ctl, drive_present, NULL);
  amiga_ctl_set_media_probes(&ctl, inspect_media, NULL, handler_pinned, NULL);
  if (!options.script[0]) {
    amiga_splash_text("Checking Wi-Fi...");
    (void) amiga_ctl_net_status_read(&ctl);
  }
  amiga_splash_wait(AMIGA_SPLASH_MIN_TICKS);
  rc = amiga_gui_run(&ctl, &options);

shutdown:
  fn_shutdown();
out:
  amiga_splash_close();
  if (IconBase)
    CloseLibrary(IconBase);
  if (GfxBase)
    CloseLibrary((struct Library *) GfxBase);
  if (IntuitionBase)
    CloseLibrary((struct Library *) IntuitionBase);
  return rc;
}
