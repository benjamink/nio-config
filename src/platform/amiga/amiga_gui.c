#include "amiga_gui.h"
#include "amiga_layout.h"
#include "amiga_theme.h"

#include <exec/types.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include <stdio.h>
#include <string.h>

extern struct IntuitionBase *IntuitionBase;

#define FONT_W 8
#define FONT_H 8

static struct TextAttr topaz8 = { (STRPTR) "topaz.font", 8, 0, 0 };
static const char *const tab_labels[AMIGA_TAB_COUNT] = {
  "Hosts", "Browse", "Catalogue", "Drives"
};

static struct Window *win;
static struct TextFont *font;
static amiga_layout_t layout;
static amiga_theme_t theme;
static amiga_ctl_t *gctl;

void amiga_gui_fatal(const char *message)
{
  static char lines[3][80];
  struct IntuiText body[3];
  struct IntuiText ok;
  const char *p;
  int n;
  int i;

  if (!IntuitionBase) {
    puts(message);
    return;
  }
  p = message;
  for (n = 0; *p && n < 3; n++) {
    i = 0;
    while (*p && *p != '\n' && i < (int) sizeof(lines[0]) - 1)
      lines[n][i++] = *p++;
    lines[n][i] = 0;
    if (*p == '\n')
      p++;
  }
  for (i = 0; i < n; i++) {
    body[i].FrontPen = 0;
    body[i].BackPen = 1;
    body[i].DrawMode = JAM1;
    body[i].LeftEdge = 8;
    body[i].TopEdge = (WORD) (4 + i * 10);
    body[i].ITextFont = &topaz8;
    body[i].IText = (UBYTE *) lines[i];
    body[i].NextText = i + 1 < n ? &body[i + 1] : NULL;
  }
  ok.FrontPen = 0;
  ok.BackPen = 1;
  ok.DrawMode = JAM1;
  ok.LeftEdge = 6;
  ok.TopEdge = 3;
  ok.ITextFont = &topaz8;
  ok.IText = (UBYTE *) "OK";
  ok.NextText = NULL;
  (void) AutoRequest(NULL, n ? &body[0] : NULL, NULL, &ok, 0, 0, 320,
                     (WORD) (50 + 10 * n));
}

static void compute_layout(const struct Screen *scr, uint8_t bl, uint8_t bt,
                           uint8_t br, uint8_t bb, int *ok)
{
  amiga_layout_in_t in;

  in.screen_w = (uint16_t) scr->Width;
  in.screen_h = (uint16_t) scr->Height;
  in.border_l = bl;
  in.border_t = bt;
  in.border_r = br;
  in.border_b = bb;
  in.font_w = FONT_W;
  in.font_h = FONT_H;
  in.gadget_font_h = (uint8_t) (scr->Font ? scr->Font->ta_YSize : FONT_H);
  *ok = amiga_layout_compute(&in, &layout);
}

static void load_theme(void)
{
  amiga_theme_classic(&theme);
#ifndef __KICK13__
  if (IntuitionBase->LibNode.lib_Version >= 36) {
    struct DrawInfo *dri = GetScreenDrawInfo(win->WScreen);

    if (dri) {
      amiga_theme_from_pens(&theme, dri->dri_Pens, dri->dri_NumPens);
      FreeScreenDrawInfo(win->WScreen, dri);
    }
  }
#endif
}

static void wait_newsize(void)
{
  struct IntuiMessage *msg;
  int done = 0;

  while (!done) {
    WaitPort(win->UserPort);
    while ((msg = (struct IntuiMessage *) GetMsg(win->UserPort)) != NULL) {
      if (msg->Class == NEWSIZE)
        done = 1;
      ReplyMsg((struct Message *) msg);
    }
  }
}

static int gui_open(void)
{
  struct Screen wb;
  struct NewWindow nw;
  int ok;

  if (!GetScreenData((APTR) &wb, sizeof(wb), WBENCHSCREEN, NULL))
    return 0;
  compute_layout(&wb, (uint8_t) wb.WBorLeft,
                 (uint8_t) (wb.WBorTop + (wb.Font ? wb.Font->ta_YSize : 8) + 1),
                 (uint8_t) wb.WBorRight, (uint8_t) wb.WBorBottom, &ok);
  if (!ok) {
    amiga_gui_fatal("The Workbench screen is too small.\n"
                    "FujiNet Config needs 640x200.");
    return 0;
  }

  memset(&nw, 0, sizeof(nw));
  nw.LeftEdge = (WORD) ((wb.Width - layout.win_w) / 2);
  nw.TopEdge = (WORD) ((wb.Height - layout.win_h) / 2);
  nw.Width = (WORD) layout.win_w;
  nw.Height = (WORD) layout.win_h;
  nw.DetailPen = (UBYTE) -1;
  nw.BlockPen = (UBYTE) -1;
  nw.IDCMPFlags = CLOSEWINDOW | GADGETUP | GADGETDOWN | MOUSEMOVE |
                  RAWKEY | MENUPICK | NEWSIZE;
  nw.Flags = WINDOWDRAG | WINDOWDEPTH | WINDOWCLOSE | ACTIVATE |
             SMART_REFRESH | NOCAREREFRESH;
  nw.Title = (UBYTE *) "FujiNet Config";
  nw.Type = WBENCHSCREEN;
  win = OpenWindow(&nw);
  if (!win) {
    amiga_gui_fatal("Unable to open the FujiNet Config window.");
    return 0;
  }

  /* Lay out from the borders the OS actually drew; Kickstarts differ. */
  compute_layout(win->WScreen, (uint8_t) win->BorderLeft,
                 (uint8_t) win->BorderTop, (uint8_t) win->BorderRight,
                 (uint8_t) win->BorderBottom, &ok);
  if (!ok) {
    amiga_gui_fatal("The Workbench screen is too small.\n"
                    "FujiNet Config needs 640x200.");
    return 0;
  }
  if (win->Width != layout.win_w || win->Height != layout.win_h) {
    WORD dx = (WORD) (layout.win_w - win->Width);
    WORD dy = (WORD) (layout.win_h - win->Height);

    if (win->TopEdge + win->Height + dy > win->WScreen->Height)
      MoveWindow(win, 0, (WORD) -(win->TopEdge + win->Height + dy -
                                  win->WScreen->Height));
    SizeWindow(win, dx, dy);
    wait_newsize();
  }

  font = OpenFont(&topaz8);
  if (font)
    SetFont(win->RPort, font);
  load_theme();
  amiga_ctl_set_rows(gctl, layout.list_rows);
  return 1;
}

static void gui_close(void)
{
  if (win) {
    CloseWindow(win);
    win = NULL;
  }
  if (font) {
    CloseFont(font);
    font = NULL;
  }
}

static void bevel(const amiga_rect_t *r, int recessed)
{
  struct RastPort *rp = win->RPort;
  WORD x0 = r->left;
  WORD y0 = r->top;
  WORD x1 = (WORD) (r->left + r->width - 1);
  WORD y1 = (WORD) (r->top + r->height - 1);

  SetAPen(rp, recessed ? theme.shadow : theme.shine);
  Move(rp, x0, y1);
  Draw(rp, x0, y0);
  Draw(rp, (WORD) (x1 - 1), y0);
  Move(rp, (WORD) (x0 + 1), (WORD) (y1 - 1));
  Draw(rp, (WORD) (x0 + 1), (WORD) (y0 + 1));
  SetAPen(rp, recessed ? theme.shine : theme.shadow);
  Move(rp, x1, y0);
  Draw(rp, x1, y1);
  Draw(rp, (WORD) (x0 + 1), y1);
  Move(rp, (WORD) (x1 - 1), (WORD) (y0 + 1));
  Draw(rp, (WORD) (x1 - 1), (WORD) (y1 - 1));
}

static void fill(const amiga_rect_t *r, uint8_t pen)
{
  SetAPen(win->RPort, pen);
  RectFill(win->RPort, r->left, r->top, (WORD) (r->left + r->width - 1),
           (WORD) (r->top + r->height - 1));
}

static void text_centred(const amiga_rect_t *r, const char *s, uint8_t pen)
{
  struct RastPort *rp = win->RPort;
  WORD len = (WORD) strlen(s);
  WORD w = TextLength(rp, (CONST_STRPTR) s, len);

  SetAPen(rp, pen);
  SetDrMd(rp, JAM1);
  Move(rp, (WORD) (r->left + (r->width - w) / 2),
       (WORD) (r->top + (r->height - FONT_H) / 2 + rp->TxBaseline));
  Text(rp, (CONST_STRPTR) s, len);
}

static void paint_tabs(void)
{
  uint8_t i;

  for (i = 0; i < AMIGA_TAB_COUNT; i++) {
    int selected = i == gctl->page;
    amiga_rect_t inner = amiga_rect_inset(layout.tab[i], 2, 1);

    fill(&inner, selected ? theme.fill : theme.background);
    bevel(&layout.tab[i], selected);
    text_centred(&layout.tab[i], tab_labels[i],
                 selected ? theme.filltext : theme.text);
  }
}

static void paint_status(void)
{
  amiga_rect_t inner = amiga_rect_inset(layout.status, 2, 1);

  fill(&inner, theme.background);
  bevel(&layout.status, 1);
  SetAPen(win->RPort, theme.text);
  Move(win->RPort, (WORD) (inner.left + 4),
       (WORD) (inner.top + 1 + win->RPort->TxBaseline));
  Text(win->RPort, (CONST_STRPTR) gctl->state->status,
       (WORD) strlen(gctl->state->status));
}

int amiga_gui_run(amiga_ctl_t *ctl, const amiga_options_t *opts)
{
  struct IntuiMessage *msg;
  int done = 0;

  (void) opts;
  gctl = ctl;
  if (!gui_open()) {
    gui_close();
    return 20;
  }
  paint_tabs();
  paint_status();
  while (!done) {
    WaitPort(win->UserPort);
    while ((msg = (struct IntuiMessage *) GetMsg(win->UserPort)) != NULL) {
      if (msg->Class == CLOSEWINDOW)
        done = 1;
      ReplyMsg((struct Message *) msg);
    }
  }
  gui_close();
  return 0;
}
