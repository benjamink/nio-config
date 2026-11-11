#include "amiga_splash.h"
#include "amiga_logo.h"
#include "amiga_theme.h"

#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include <string.h>

#define PAD_X 24
#define PAD_TOP 14
#define GAP 12
#define FONT_W 8
#define FONT_H 8
#define WIN_W (AMIGA_SPLASH_W + 2 * PAD_X)
#define WIN_H (PAD_TOP + AMIGA_SPLASH_H + GAP + FONT_H + PAD_TOP)
#define MASK_BYTES (AMIGA_SPLASH_WORDS * 2 * AMIGA_SPLASH_H)

static struct TextAttr topaz8 = { (STRPTR) "topaz.font", 8, 0, 0 };

static struct Window *win;
static struct TextFont *font;
static UWORD *chip;            /* emblem mask then lettering mask */
static amiga_theme_t theme;
static ULONG open_secs;
static ULONG open_micros;

static void rect_fill(WORD x, WORD y, WORD w, WORD h, UBYTE pen)
{
  SetAPen(win->RPort, pen);
  RectFill(win->RPort, x, y, (WORD) (x + w - 1), (WORD) (y + h - 1));
}

/* Raised border, two pixels wide on the left and right as on 2.0 hires. */
static void bevel(WORD x0, WORD y0, WORD x1, WORD y1)
{
  struct RastPort *rp = win->RPort;

  SetAPen(rp, theme.shine);
  Move(rp, x0, y1);
  Draw(rp, x0, y0);
  Draw(rp, (WORD) (x1 - 1), y0);
  Move(rp, (WORD) (x0 + 1), (WORD) (y1 - 1));
  Draw(rp, (WORD) (x0 + 1), (WORD) (y0 + 1));
  SetAPen(rp, theme.shadow);
  Move(rp, x1, y0);
  Draw(rp, x1, y1);
  Draw(rp, (WORD) (x0 + 1), y1);
  Move(rp, (WORD) (x1 - 1), (WORD) (y0 + 1));
  Draw(rp, (WORD) (x1 - 1), (WORD) (y1 - 1));
}

void amiga_splash_text(const char *text)
{
  WORD len;
  WORD w;
  WORD y;

  if (!win)
    return;
  y = (WORD) (PAD_TOP + AMIGA_SPLASH_H + GAP);
  rect_fill(4, y, (WORD) (WIN_W - 8), FONT_H, theme.background);
  len = (WORD) strlen(text);
  if (len > AMIGA_SPLASH_TEXT_MAX)
    len = AMIGA_SPLASH_TEXT_MAX;
  w = (WORD) (len * FONT_W);
  SetAPen(win->RPort, theme.text);
  SetDrMd(win->RPort, JAM1);
  Move(win->RPort, (WORD) ((WIN_W - w) / 2), (WORD) (y + win->RPort->TxBaseline));
  Text(win->RPort, (CONST_STRPTR) text, len);
}

void amiga_splash_open(const char *text)
{
  struct Screen wb;
  struct NewWindow nw;
  struct RastPort *rp;
  WORD x;

  if (win)
    return;
  if (!GetScreenData((APTR) &wb, sizeof(wb), WBENCHSCREEN, NULL) ||
      wb.Width < WIN_W || wb.Height < WIN_H)
    return;

  /* The mask copies live in chip RAM: BltTemplate reads them by blitter. */
  chip = (UWORD *) AllocMem(2 * MASK_BYTES, MEMF_CHIP);
  if (!chip)
    return;
  CopyMem((APTR) amiga_splash_emblem, chip, MASK_BYTES);
  CopyMem((APTR) amiga_splash_lettering, (UBYTE *) chip + MASK_BYTES, MASK_BYTES);

  memset(&nw, 0, sizeof(nw));
  nw.LeftEdge = (WORD) ((wb.Width - WIN_W) / 2);
  nw.TopEdge = (WORD) ((wb.Height - WIN_H) / 2);
  nw.Width = WIN_W;
  nw.Height = WIN_H;
  nw.DetailPen = (UBYTE) -1;
  nw.BlockPen = (UBYTE) -1;
  nw.Flags = BORDERLESS | ACTIVATE | SMART_REFRESH | NOCAREREFRESH;
  nw.Type = WBENCHSCREEN;
  win = OpenWindow(&nw);
  if (!win) {
    FreeMem(chip, 2 * MASK_BYTES);
    chip = NULL;
    return;
  }
  CurrentTime(&open_secs, &open_micros);

  amiga_theme_for_version(&theme, (uint16_t) IntuitionBase->LibNode.lib_Version);
  if (IntuitionBase->LibNode.lib_Version >= 36) {
    struct DrawInfo *dri = GetScreenDrawInfo(win->WScreen);

    if (dri) {
      amiga_theme_from_pens(&theme, dri->dri_Pens, dri->dri_NumPens);
      FreeScreenDrawInfo(win->WScreen, dri);
    }
  }
  font = OpenFont(&topaz8);
  rp = win->RPort;
  if (font)
    SetFont(rp, font);

  rect_fill(0, 0, WIN_W, WIN_H, theme.background);
  bevel(0, 0, (WORD) (WIN_W - 1), (WORD) (WIN_H - 1));
  x = (WORD) ((WIN_W - AMIGA_SPLASH_W) / 2);
  /* Emblem in the fill pen and lettering in the text pen, as in the
   * window header, so it suits both the 1.3 and the 2.x/3.x palettes. */
  SetDrMd(rp, JAM1);
  SetAPen(rp, theme.fill);
  BltTemplate((PLANEPTR) chip, 0, AMIGA_SPLASH_WORDS * 2, rp, x, PAD_TOP,
              AMIGA_SPLASH_W, AMIGA_SPLASH_H);
  SetAPen(rp, theme.text);
  BltTemplate((PLANEPTR) ((UBYTE *) chip + MASK_BYTES), 0,
              AMIGA_SPLASH_WORDS * 2, rp, x, PAD_TOP, AMIGA_SPLASH_W,
              AMIGA_SPLASH_H);
  amiga_splash_text(text);
}

void amiga_splash_wait(unsigned ticks)
{
  ULONG secs;
  ULONG micros;
  LONG elapsed;

  if (!win)
    return;
  CurrentTime(&secs, &micros);
  elapsed = (LONG) (secs - open_secs) * 50L +
            ((LONG) micros - (LONG) open_micros) / 20000L;
  if (elapsed < (LONG) ticks)
    Delay((ULONG) ((LONG) ticks - elapsed));
}

void amiga_splash_close(void)
{
  if (win) {
    CloseWindow(win);
    win = NULL;
  }
  if (font) {
    CloseFont(font);
    font = NULL;
  }
  if (chip) {
    FreeMem(chip, 2 * MASK_BYTES);
    chip = NULL;
  }
}
