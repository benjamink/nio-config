/*
 * Intuition front end for config-nio.  Uses only the V33 (Kickstart 1.3)
 * gadget, menu and requester structures; DrawInfo pens and new-look props
 * are used when running on V36+ and the build is not the WB1.3 profile.
 * All state changes go through amiga_ctl, which the SCRIPT= driver shares.
 */
#include "amiga_gui.h"
#include "amiga_drives.h"
#include "amiga_format.h"
#include "amiga_help.h"
#include "amiga_input.h"
#include "amiga_layout.h"
#include "amiga_logo.h"
#include "amiga_script.h"
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
#ifndef __KICK13__
#include <proto/wb.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct IntuitionBase *IntuitionBase;
#ifndef __KICK13__
struct Library *WorkbenchBase;
#endif

#define FONT_W 8
#define FONT_H 8
#define ROW_TEXT_MAX 128

enum {
  GID_TAB0 = 0,
  GID_LIST = GID_TAB0 + AMIGA_TAB_COUNT,
  GID_PROP,
  GID_EDIT,
  GID_SLOT,
  GID_RO,
  GID_BTN0
};
#define GID_COUNT (GID_BTN0 + AMIGA_BUTTON_COUNT)

enum {
  ACT_NONE = 0,
  ACT_HOST_BROWSE,
  ACT_HOST_ADD,
  ACT_HOST_REPLACE,
  ACT_HOST_REMOVE,
  ACT_HOST_UP,
  ACT_HOST_DOWN,
  ACT_BROWSE_OPEN,
  ACT_BROWSE_PARENT,
  ACT_BROWSE_REFRESH,
  ACT_BROWSE_MOUNT,
  ACT_SLOT_SET,
  ACT_SLOT_CLEAR,
  ACT_SLOT_MOUNT,
  ACT_DRIVE_EJECT,
  ACT_DRIVE_REMOUNT,
  ACT_MOUNT_COMMIT,
  ACT_MOUNT_CANCEL,
  ACT_HELP_CONTENTS,
  ACT_HELP_PREV,
  ACT_HELP_NEXT,
  ACT_HELP_CLOSE
};

typedef struct {
  const char *label;
  uint8_t action;
} gui_button_t;

/* Catalogue is reached from the Project menu; the mount picker from
 * Browse/Catalogue.  Neither has a page button. */
static const char *const tab_labels[AMIGA_TAB_COUNT] = {
  "Hosts", "Browse", "Drives"
};
static const uint8_t tab_pages[AMIGA_TAB_COUNT] = {
  AMIGA_PAGE_HOSTS, AMIGA_PAGE_BROWSE, AMIGA_PAGE_DRIVES
};

static const gui_button_t page_buttons[AMIGA_PAGE_COUNT][AMIGA_BUTTON_COUNT] = {
  { { "Browse", ACT_HOST_BROWSE }, { "Add", ACT_HOST_ADD },
    { "Replace", ACT_HOST_REPLACE }, { "Remove", ACT_HOST_REMOVE },
    { "Move Up", ACT_HOST_UP }, { "Move Down", ACT_HOST_DOWN } },
  { { "Open", ACT_BROWSE_OPEN }, { "Parent", ACT_BROWSE_PARENT },
    { "Refresh", ACT_BROWSE_REFRESH }, { "Mount...", ACT_BROWSE_MOUNT },
    { NULL, ACT_NONE }, { NULL, ACT_NONE } },
  { { "Set", ACT_SLOT_SET }, { "Clear", ACT_SLOT_CLEAR },
    { "Mount...", ACT_SLOT_MOUNT }, { NULL, ACT_NONE }, { NULL, ACT_NONE },
    { NULL, ACT_NONE } },
  { { "Eject", ACT_DRIVE_EJECT }, { "Remount", ACT_DRIVE_REMOUNT },
    { NULL, ACT_NONE }, { NULL, ACT_NONE }, { NULL, ACT_NONE },
    { NULL, ACT_NONE } },
  { { "Mount", ACT_MOUNT_COMMIT }, { NULL, ACT_NONE },
    { NULL, ACT_NONE }, { NULL, ACT_NONE }, { NULL, ACT_NONE },
    { "Cancel", ACT_MOUNT_CANCEL } },
  { { "Contents", ACT_HELP_CONTENTS }, { "Previous", ACT_HELP_PREV },
    { "Next", ACT_HELP_NEXT }, { NULL, ACT_NONE }, { NULL, ACT_NONE },
    { "Close", ACT_HELP_CLOSE } },
};

/* RKM wait pointer image; sprite data must live in chip RAM. */
static const UWORD busy_image[] = {
  0x0000, 0x0000,
  0x0400, 0x07C0, 0x0000, 0x07C0, 0x0100, 0x0380, 0x0000, 0x07E0,
  0x07C0, 0x1FF8, 0x1FF0, 0x3FEC, 0x3FF8, 0x7FDE, 0x3FF8, 0x7FBE,
  0x7FFC, 0xFF7F, 0x7EFC, 0xFFFF, 0x7FFC, 0xFFFF, 0x3FF8, 0x7FFE,
  0x3FF8, 0x7FFE, 0x1FF0, 0x3FFC, 0x07C0, 0x1FF8, 0x0000, 0x07E0,
  0x0000, 0x0000
};

static struct TextAttr topaz8 = { (STRPTR) "topaz.font", 8, 0, 0 };

static struct Window *win;
static struct TextFont *font;
static amiga_layout_t layout;
static amiga_theme_t theme;
static amiga_ctl_t *gctl;
static uint8_t new_look;

static struct Gadget gad[GID_COUNT];
static uint8_t attached[GID_COUNT];
static struct StringInfo edit_si;
static struct StringInfo slot_si;
static struct PropInfo prop_pi;
static struct Image prop_knob;
static UBYTE edit_buf[CONFIG_NIO_URI_MAX + 1];
static UBYTE edit_undo[CONFIG_NIO_URI_MAX + 1];
static UBYTE slot_buf[4];
static UBYTE slot_undo[4];
static uint8_t prop_active;

static UWORD *busy_sprite;
/* Chip-RAM copies of the logo masks; BltTemplate reads them by blitter. */
static UWORD *logo_chip;
#define LOGO_MASK_BYTES (sizeof(amiga_logo_emblem))
static struct Requester block_req;
static uint8_t busy_depth;

static ULONG last_secs;
static ULONG last_micros;
static uint16_t last_index = AMIGA_LIST_NONE;

/* Wrapped lines of the help topic being shown. */
#define HELP_LINES_MAX 400
static amiga_help_line_t help_lines[HELP_LINES_MAX];

static char row_text[ROW_TEXT_MAX + 1];
static char tmp_text[CONFIG_NIO_URI_MAX + ROW_TEXT_MAX + 32];
static char uri_text[CONFIG_NIO_URI_MAX + 1];

/* ---- Menus ------------------------------------------------------------ */

static struct IntuiText menu_text[7 + AMIGA_HELP_TOPICS];
static struct MenuItem project_items[3];
static struct MenuItem settings_items[4];
static struct MenuItem help_items[AMIGA_HELP_TOPICS];
static struct Menu menus[3];
static const char *const project_labels[3] = {
  "Catalogue...", "About...", "Quit"
};
static const char project_keys[3] = { 'C', '?', 'Q' };
static const char *const settings_labels[4] = {
  "Dates YY-MM-DD", "Dates YY-DD-MM", "Sizes Full", "Sizes Compact"
};
static uint8_t menus_attached;

/* ---- Small drawing helpers ------------------------------------------- */

static void fill(const amiga_rect_t *r, uint8_t pen)
{
  if (r->width <= 0 || r->height <= 0)
    return;
  SetAPen(win->RPort, pen);
  RectFill(win->RPort, r->left, r->top, (WORD) (r->left + r->width - 1),
           (WORD) (r->top + r->height - 1));
}

/* 2.0 hires style: double left edge, single top edge. */
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

static void text_at(WORD x, WORD top, const char *s, WORD len, uint8_t pen)
{
  struct RastPort *rp = win->RPort;

  SetAPen(rp, pen);
  SetDrMd(rp, JAM1);
  Move(rp, x, (WORD) (top + rp->TxBaseline));
  Text(rp, (CONST_STRPTR) s, len);
}

static void text_centred(const amiga_rect_t *r, const char *s, uint8_t pen)
{
  WORD len = (WORD) strlen(s);
  WORD w = TextLength(win->RPort, (CONST_STRPTR) s, len);

  text_at((WORD) (r->left + (r->width - w) / 2),
          (WORD) (r->top + (r->height - FONT_H) / 2), s, len, pen);
}

/* Copies `s` into row_text padded or clipped to exactly `cols` characters. */
static const char *padded(const char *s, uint8_t cols)
{
  uint8_t i;

  for (i = 0; i < cols && i < ROW_TEXT_MAX && s[i]; i++)
    row_text[i] = s[i];
  for (; i < cols && i < ROW_TEXT_MAX; i++)
    row_text[i] = ' ';
  row_text[i] = 0;
  return row_text;
}

/* ---- Requesters -------------------------------------------------------- */

static void make_itext(struct IntuiText *t, WORD left, WORD top,
                       const char *s, struct IntuiText *next)
{
  t->FrontPen = 0;
  t->BackPen = 1;
  t->DrawMode = JAM1;
  t->LeftEdge = left;
  t->TopEdge = top;
  t->ITextFont = &topaz8;
  t->IText = (UBYTE *) s;
  t->NextText = next;
}

static int requester(struct Window *w, const char *message, const char *yes,
                     const char *no)
{
  static char lines[3][80];
  struct IntuiText body[3];
  struct IntuiText pos;
  struct IntuiText neg;
  const char *p;
  int n;
  int i;

  p = message;
  for (n = 0; *p && n < 3; n++) {
    i = 0;
    while (*p && *p != '\n' && i < (int) sizeof(lines[0]) - 1)
      lines[n][i++] = *p++;
    lines[n][i] = 0;
    if (*p == '\n')
      p++;
  }
  for (i = 0; i < n; i++)
    make_itext(&body[i], 8, (WORD) (4 + i * 10), lines[i],
               i + 1 < n ? &body[i + 1] : NULL);
  make_itext(&pos, 6, 3, yes ? yes : "", NULL);
  make_itext(&neg, 6, 3, no, NULL);
  return (int) AutoRequest(w, n ? &body[0] : NULL, yes ? &pos : NULL, &neg,
                           0, 0, 320, (WORD) (50 + 10 * n));
}

void amiga_gui_fatal(const char *message)
{
  if (!IntuitionBase) {
    puts(message);
    return;
  }
  (void) requester(win, message, NULL, "OK");
}

static int confirm(const char *message)
{
  return requester(win, message, "Yes", "No");
}

static void about(void)
{
  (void) requester(win, "FujiNet Config\nHosts, catalogue and drives\n"
                   "for FujiNet NIO on the Amiga", NULL, "OK");
}

/* ---- Busy state ---------------------------------------------------------- */

static void busy_begin(void)
{
  if (busy_depth++ != 0)
    return;
  InitRequester(&block_req);
  (void) Request(&block_req, win);
  if (busy_sprite)
    SetPointer(win, busy_sprite, 16, 16, -6, 0);
}

static void busy_end(void)
{
  if (busy_depth == 0 || --busy_depth != 0)
    return;
  ClearPointer(win);
  EndRequest(&block_req, win);
}

/* ---- Gadgets ------------------------------------------------------------ */

static void make_gadget(uint8_t id, amiga_rect_t r, UWORD flags,
                        UWORD activation, UWORD type)
{
  struct Gadget *g = &gad[id];

  memset(g, 0, sizeof(*g));
  g->LeftEdge = r.left;
  g->TopEdge = r.top;
  g->Width = r.width;
  g->Height = r.height;
  g->Flags = flags;
  g->Activation = activation;
  g->GadgetType = type;
  g->GadgetID = id;
}

static void gui_make_gadgets(void)
{
  uint8_t i;

  for (i = 0; i < AMIGA_TAB_COUNT; i++)
    make_gadget((uint8_t) (GID_TAB0 + i), layout.tab[i], GADGHCOMP,
                RELVERIFY, BOOLGADGET);
  for (i = 0; i < AMIGA_BUTTON_COUNT; i++)
    make_gadget((uint8_t) (GID_BTN0 + i), layout.button[i], GADGHCOMP,
                RELVERIFY, BOOLGADGET);
  make_gadget(GID_LIST, amiga_rect_inset(layout.list, 2, 2), GADGHNONE,
              GADGIMMEDIATE, BOOLGADGET);

  memset(&prop_pi, 0, sizeof(prop_pi));
  prop_pi.Flags = (UWORD) (AUTOKNOB | FREEVERT | (new_look ? PROPNEWLOOK : 0));
  prop_pi.HorizBody = MAXBODY;
  prop_pi.VertBody = MAXBODY;
  make_gadget(GID_PROP, layout.scroller, GADGHNONE,
              GADGIMMEDIATE | RELVERIFY | FOLLOWMOUSE, PROPGADGET);
  gad[GID_PROP].GadgetRender = (APTR) &prop_knob;
  gad[GID_PROP].SpecialInfo = (APTR) &prop_pi;

  memset(&edit_si, 0, sizeof(edit_si));
  edit_si.Buffer = edit_buf;
  edit_si.UndoBuffer = edit_undo;
  edit_si.MaxChars = sizeof(edit_buf);
  make_gadget(GID_EDIT, amiga_rect_inset(layout.edit, 4, 3), GADGHCOMP,
              RELVERIFY, STRGADGET);
  gad[GID_EDIT].SpecialInfo = (APTR) &edit_si;

  memset(&slot_si, 0, sizeof(slot_si));
  slot_si.Buffer = slot_buf;
  slot_si.UndoBuffer = slot_undo;
  slot_si.MaxChars = sizeof(slot_buf);
  strcpy((char *) slot_buf, "0");
  make_gadget(GID_SLOT, amiga_rect_inset(layout.slot, 4, 3), GADGHCOMP,
              RELVERIFY | LONGINT, STRGADGET);
  gad[GID_SLOT].SpecialInfo = (APTR) &slot_si;

  make_gadget(GID_RO, layout.ro, GADGHNONE, TOGGLESELECT | RELVERIFY,
              BOOLGADGET);
  gad[GID_RO].Flags |= SELECTED;   /* mounting defaults to read-only */
}

static void attach(uint8_t id)
{
  if (attached[id])
    return;
  (void) AddGadget(win, &gad[id], (UWORD) ~0);
  attached[id] = 1;
}

static void detach(uint8_t id)
{
  if (!attached[id])
    return;
  (void) RemoveGadget(win, &gad[id]);
  attached[id] = 0;
}

static int page_has(uint8_t id)
{
  /* Help replaces the page buttons with a heading, so no page looks
   * selected while it is showing. */
  if (id < GID_TAB0 + AMIGA_TAB_COUNT)
    return gctl->page != AMIGA_PAGE_HELP;
  switch (id) {
  case GID_EDIT:
    return gctl->page == AMIGA_PAGE_HOSTS ||
           gctl->page == AMIGA_PAGE_CATALOGUE;
  case GID_SLOT:
    return gctl->page == AMIGA_PAGE_CATALOGUE;
  case GID_RO:
    return gctl->page == AMIGA_PAGE_CATALOGUE ||
           gctl->page == AMIGA_PAGE_MOUNT;
  default:
    return 1;
  }
}

/* Attaches exactly the gadgets the current page shows. */
static void gui_sync_gadgets(void)
{
  uint8_t id;

  for (id = 0; id < GID_COUNT; id++) {
    if (page_has(id))
      attach(id);
    else
      detach(id);
  }
}

static void set_string(uint8_t id, const char *text)
{
  struct StringInfo *si = (struct StringInfo *) gad[id].SpecialInfo;
  UWORD pos = 0;
  int was_attached = attached[id];

  if (was_attached)
    pos = RemoveGadget(win, &gad[id]);
  strncpy((char *) si->Buffer, text, (size_t) si->MaxChars - 1);
  si->Buffer[si->MaxChars - 1] = 0;
  si->BufferPos = 0;
  si->DispPos = 0;
  if (id == GID_SLOT)
    si->LongInt = atol(text);
  if (was_attached) {
    (void) AddGadget(win, &gad[id], pos);
    RefreshGList(&gad[id], win, NULL, 1);
  }
}

static void set_ro(int readonly)
{
  UWORD pos = 0;
  int was_attached = attached[GID_RO];

  if (was_attached)
    pos = RemoveGadget(win, &gad[GID_RO]);
  if (readonly)
    gad[GID_RO].Flags |= SELECTED;
  else
    gad[GID_RO].Flags &= (UWORD) ~SELECTED;
  if (was_attached)
    (void) AddGadget(win, &gad[GID_RO], pos);
}

static int read_slot(uint8_t *slot)
{
  char *end;
  long v = strtol((const char *) slot_buf, &end, 10);

  if (!slot_buf[0] || *end || v < 0 || v > 255) {
    config_nio_set_status(gctl->state, "Slot must be 0-255");
    return 0;
  }
  *slot = (uint8_t) v;
  return 1;
}

static int read_ro(void)
{
  return (gad[GID_RO].Flags & SELECTED) != 0;
}

/* ---- Painting ---------------------------------------------------------- */

static amiga_list_t *page_list(void)
{
  switch (gctl->page) {
  case AMIGA_PAGE_BROWSE:
    return &gctl->entries;
  case AMIGA_PAGE_CATALOGUE:
    return &gctl->catalogue;
  case AMIGA_PAGE_DRIVES:
  case AMIGA_PAGE_MOUNT:
    return &gctl->drives;
  case AMIGA_PAGE_HELP:
    return &gctl->help;
  default:
    return &gctl->hosts;
  }
}

static amiga_rect_t list_interior(void)
{
  return amiga_rect_inset(layout.list, 2, 2);
}

static uint8_t list_cols(void)
{
  amiga_rect_t in = list_interior();
  int cols = (in.width - 4) / FONT_W;

  return (uint8_t) (cols > ROW_TEXT_MAX ? ROW_TEXT_MAX : cols);
}

static const char *slot_uri(uint8_t slot, const char **mode)
{
  const config_nio_slot_t *s = amiga_ctl_slot(gctl, slot);

  if (!s) {
    *mode = "  ";
    return "?";
  }
  if (!s->enabled || !s->uri[0]) {
    *mode = "  ";
    return "";
  }
  *mode = strcmp(s->mode, "r") == 0 ? "RO" : "RW";
  return s->uri;
}

static void row_string(uint16_t idx, uint8_t cols)
{
  config_nio_state_t *s = gctl->state;
  char size_buf[AMIGA_SIZE_TEXT_MAX];
  char date_buf[AMIGA_DATE_TEXT_MAX];
  const char *mode;
  const char *uri;

  switch (gctl->page) {
  case AMIGA_PAGE_HELP:
    if (gctl->help_topic == AMIGA_HELP_CONTENTS) {
      sprintf(tmp_text, "  %s", amiga_help_title((uint8_t) (idx + 1)));
    } else {
      const amiga_help_line_t *hl = &help_lines[idx];

      memset(tmp_text, ' ', hl->indent);
      memcpy(tmp_text + hl->indent,
             amiga_help_text(gctl->help_topic) + hl->start, hl->len);
      tmp_text[hl->indent + hl->len] = 0;
    }
    break;
  case AMIGA_PAGE_HOSTS:
    amiga_clip_head(uri_text, sizeof(uri_text), s->hosts[idx],
                    (uint8_t) (cols - 3));
    sprintf(tmp_text, "%2u %s", (unsigned) idx, uri_text);
    break;
  case AMIGA_PAGE_BROWSE: {
    config_nio_entry_t *e = &s->entries[idx];
    uint8_t name_w = (uint8_t) (cols > 24 ? cols - 24 : 1);

    amiga_clip_head(uri_text, sizeof(uri_text), e->name, name_w);
    if (e->is_dir & CONFIG_NIO_ENTRY_FLAG_DIR)
      strcpy(size_buf, "Drawer");
    else
      amiga_format_size(size_buf, e->size, s->prefs.size_format);
    (void) amiga_format_date(date_buf, e->mtime, s->prefs.date_format);
    strcpy(tmp_text, padded(uri_text, name_w));
    sprintf(tmp_text + strlen(tmp_text), " %13s %8s", size_buf, date_buf);
    break;
  }
  case AMIGA_PAGE_CATALOGUE:
    uri = slot_uri((uint8_t) idx, &mode);
    amiga_clip_head(uri_text, sizeof(uri_text), uri, (uint8_t) (cols - 8));
    sprintf(tmp_text, "%3u %s  %s", (unsigned) idx, mode, uri_text);
    break;
  default: {
    config_nio_mapping_t m;
    const char *label = amiga_drive_label((uint8_t) idx, gctl->kick13);

    if (config_nio_mapping_get(s, (uint8_t) idx, &m) && m.valid) {
      const config_nio_slot_t *ds = amiga_ctl_drive_slot(gctl, (uint8_t) idx);

      const char *name;

      uri = !ds ? "?" : (ds->enabled ? ds->uri : "");
      name = strrchr(uri, '/');
      name = name && name[1] ? name + 1 : uri;
      int saved = amiga_ctl_drive_state(gctl, (uint8_t) idx) ==
                  AMIGA_DRIVE_SAVED;

      amiga_clip_head(uri_text, sizeof(uri_text), name,
                      (uint8_t) (cols - (saved ? 27 : 12)));
      sprintf(tmp_text, "%-5s %s  %s%s", label, m.readonly ? "RO" : "RW",
              uri_text, saved ? "  (not mounted)" : "");
    } else {
      sprintf(tmp_text, "%-5s (empty)", label);
    }
    break;
  }
  }
}

static void update_prop(void)
{
  uint16_t pot;
  uint16_t body;

  amiga_list_prop(page_list(), &pot, &body);
  NewModifyProp(&gad[GID_PROP], win, NULL, prop_pi.Flags, 0, pot, MAXBODY,
                body, 1);
}

static void gui_paint_rows(void)
{
  amiga_list_t *l = page_list();
  amiga_rect_t in = list_interior();
  uint8_t cols = list_cols();
  uint8_t r;

  for (r = 0; r < layout.list_rows; r++) {
    amiga_rect_t row;
    uint16_t idx = (uint16_t) (l->top + r);
    int selected = idx == l->selected &&
                   (gctl->page != AMIGA_PAGE_HELP ||
                    gctl->help_topic == AMIGA_HELP_CONTENTS);

    row.left = in.left;
    row.top = (int16_t) (in.top + r * layout.row_h);
    row.width = in.width;
    row.height = layout.row_h;
    if (gctl->page == AMIGA_PAGE_BROWSE && !gctl->browse_open)
      idx = l->count;
    if (idx >= l->count) {
      fill(&row, theme.background);
      continue;
    }
    fill(&row, selected ? theme.fill : theme.background);
    row_string(idx, cols);
    text_at((WORD) (row.left + 2), (WORD) (row.top + 1), padded(tmp_text, cols),
            cols, selected ? theme.filltext : theme.text);
  }
}

static void gui_paint_info(void)
{
  config_nio_state_t *s = gctl->state;
  uint8_t cols = (uint8_t) (layout.info.width / FONT_W);

  fill(&layout.info, theme.background);
  switch (gctl->page) {
  case AMIGA_PAGE_HOSTS:
    sprintf(tmp_text, "FujiNet hosts (%u of %u)", (unsigned) s->host_count,
            (unsigned) CONFIG_NIO_MAX_HOSTS);
    break;
  case AMIGA_PAGE_BROWSE:
    if (!gctl->browse_open || gctl->browse_host >= s->host_count) {
      strcpy(tmp_text, "Choose a host and press Browse");
    } else {
      /* host (<=255) + '/' + path (<=127) fits tmp_text, not uri_text. */
      sprintf(tmp_text, "%s/%s", s->hosts[gctl->browse_host], s->browse_path);
      amiga_clip_tail(uri_text, sizeof(uri_text), tmp_text, cols);
      strcpy(tmp_text, uri_text);
    }
    break;
  case AMIGA_PAGE_CATALOGUE:
    strcpy(tmp_text, "Catalogue: Slot Mode Image");
    break;
  case AMIGA_PAGE_HELP:
    sprintf(tmp_text, "Help: %s", amiga_help_title(gctl->help_topic));
    break;
  case AMIGA_PAGE_MOUNT:
    amiga_clip_head(uri_text, sizeof(uri_text), gctl->mount_name,
                    (uint8_t) (cols - 16));
    sprintf(tmp_text, "Mount %s on drive:", uri_text);
    break;
  default:
    strcpy(tmp_text, "Drive Mode Image");
    break;
  }
  text_at(layout.info.left, (WORD) (layout.info.top + 1), tmp_text,
          (WORD) strlen(tmp_text), theme.highlight);
}

static void gui_paint_status(void)
{
  amiga_rect_t in = amiga_rect_inset(layout.status, 2, 1);
  uint8_t cols = (uint8_t) ((in.width - 8) / FONT_W);

  fill(&in, theme.background);
  bevel(&layout.status, 1);
  amiga_clip_head(tmp_text, sizeof(tmp_text), gctl->state->status, cols);
  text_at((WORD) (in.left + 4), (WORD) (in.top + 1), tmp_text,
          (WORD) strlen(tmp_text), theme.text);
}

static void paint_checkbox(void)
{
  struct RastPort *rp = win->RPort;
  amiga_rect_t in = amiga_rect_inset(layout.ro, 2, 1);
  WORD cx;
  WORD cy;

  fill(&in, theme.background);
  bevel(&layout.ro, 1);
  if (!read_ro())
    return;
  cx = (WORD) (layout.ro.left + 4);
  cy = (WORD) (layout.ro.top + layout.ro.height / 2);
  SetAPen(rp, theme.text);
  Move(rp, cx, cy);
  Draw(rp, (WORD) (cx + 3), (WORD) (cy + 3));
  Draw(rp, (WORD) (cx + 9), (WORD) (cy - 4));
  Move(rp, (WORD) (cx + 1), cy);
  Draw(rp, (WORD) (cx + 4), (WORD) (cy + 3));
  Draw(rp, (WORD) (cx + 10), (WORD) (cy - 4));
}

static void gui_paint_buttons(void)
{
  uint8_t i;

  for (i = 0; i < AMIGA_BUTTON_COUNT; i++) {
    const gui_button_t *b = &page_buttons[gctl->page][i];
    amiga_rect_t in = amiga_rect_inset(layout.button[i], 2, 1);
    const char *label = b->label;

    if (b->action == ACT_MOUNT_COMMIT &&
        gctl->drives.selected != AMIGA_LIST_NONE &&
        amiga_ctl_drive_mounted(gctl, (uint8_t) gctl->drives.selected))
      label = "Replace";
    fill(&layout.button[i], theme.background);
    if (!label)
      continue;
    fill(&in, theme.background);
    bevel(&layout.button[i], 0);
    text_centred(&layout.button[i], label, theme.text);
  }
}

static uint8_t current_tab(void);

static void gui_paint_tabs(void)
{
  uint8_t tab = current_tab();
  uint8_t i;

  if (gctl->page == AMIGA_PAGE_HELP) {
    amiga_rect_t row = layout.tab[0];

    row.width = (int16_t) (layout.tab[AMIGA_TAB_COUNT - 1].left +
                           layout.tab[AMIGA_TAB_COUNT - 1].width - row.left);
    fill(&row, theme.background);
    text_centred(&row, "FujiNet Config Help", theme.highlight);
    return;
  }

  for (i = 0; i < AMIGA_TAB_COUNT; i++) {
    int selected = i == tab;
    amiga_rect_t in = amiga_rect_inset(layout.tab[i], 2, 1);

    fill(&in, selected ? theme.fill : theme.background);
    bevel(&layout.tab[i], selected);
    text_centred(&layout.tab[i], tab_labels[i],
                 selected ? theme.filltext : theme.text);
  }
}

/* Emblem in the fill pen, lettering in the text pen: orange/white on a
 * 1.3 Workbench, the DrawInfo pens (e.g. blue/black) on 2.0 and later. */
static void gui_paint_logo(void)
{
  struct RastPort *rp = win->RPort;
  WORD x;
  WORD y;

  if (!logo_chip || layout.logo.width == 0)
    return;
  fill(&layout.logo, theme.background);
  x = layout.logo.left;
  y = (WORD) (layout.logo.top + (layout.logo.height - AMIGA_LOGO_H) / 2);
  SetDrMd(rp, JAM1);
  SetAPen(rp, theme.fill);
  BltTemplate((PLANEPTR) logo_chip, 0, AMIGA_LOGO_WORDS * 2, rp, x, y,
              AMIGA_LOGO_W, AMIGA_LOGO_H);
  SetAPen(rp, theme.text);
  BltTemplate((PLANEPTR) ((UBYTE *) logo_chip + LOGO_MASK_BYTES), 0,
              AMIGA_LOGO_WORDS * 2, rp, x, y, AMIGA_LOGO_W, AMIGA_LOGO_H);
}

static void gui_paint_editors(void)
{
  WORD label_top = (WORD) (layout.edit.top + (layout.edit.height - FONT_H) / 2);

  if (attached[GID_EDIT]) {
    bevel(&layout.edit, 1);
    text_at((WORD) (layout.edit.left - 4 * FONT_W - 4), label_top, "URI", 3,
            theme.text);
  }
  if (attached[GID_SLOT]) {
    bevel(&layout.slot, 1);
    text_at((WORD) (layout.slot.left - 4 * FONT_W - 4), label_top, "Slot", 4,
            theme.text);
  }
  if (attached[GID_RO]) {
    paint_checkbox();
    text_at((WORD) (layout.ro.left + layout.ro.width + 4), label_top, "RO", 2,
            theme.text);
  }
}

/* Full repaint: clears the window interior, then redraws everything. */
static void gui_paint(void)
{
  amiga_rect_t inner;

  inner.left = (int16_t) win->BorderLeft;
  inner.top = (int16_t) win->BorderTop;
  inner.width = (int16_t) (win->Width - win->BorderLeft - win->BorderRight);
  inner.height = (int16_t) (win->Height - win->BorderTop - win->BorderBottom);
  fill(&inner, theme.background);
  gui_paint_logo();
  gui_paint_tabs();
  gui_paint_info();
  bevel(&layout.list, 1);
  gui_paint_rows();
  gui_paint_editors();
  gui_paint_buttons();
  gui_paint_status();
  RefreshGadgets(win->FirstGadget, win, NULL);
  update_prop();
}

/* Copies the selected row's values into the editing gadgets. */
static void gui_sync_editors(void)
{
  config_nio_state_t *s = gctl->state;
  amiga_list_t *l = page_list();
  char num[8];

  if (l->selected == AMIGA_LIST_NONE)
    return;
  switch (gctl->page) {
  case AMIGA_PAGE_HOSTS:
    set_string(GID_EDIT, s->hosts[l->selected]);
    break;
  case AMIGA_PAGE_CATALOGUE: {
    const config_nio_slot_t *slot = amiga_ctl_slot(gctl, (uint8_t) l->selected);

    sprintf(num, "%u", (unsigned) l->selected);
    set_string(GID_SLOT, num);
    set_string(GID_EDIT, slot && slot->enabled ? slot->uri : "");
    set_ro(amiga_ctl_catalogue_readonly(gctl, (uint8_t) l->selected));
    break;
  }
  default:
    break;
  }
  if (attached[GID_RO])
    paint_checkbox();
}

/* Lays out the current help topic for the list width.  Contents lists the
 * other topics' titles; a topic lists its wrapped text lines. */
static void gui_help_sync(void)
{
  uint16_t count;

  if (gctl->page != AMIGA_PAGE_HELP)
    return;
  if (gctl->help_topic == AMIGA_HELP_CONTENTS) {
    count = AMIGA_HELP_TOPICS - 1;
    config_nio_set_status(gctl->state,
                          "Double-click a topic to read it");
  } else {
    count = amiga_help_layout(amiga_help_text(gctl->help_topic),
                              list_cols(), help_lines, HELP_LINES_MAX);
    sprintf(tmp_text, "Topic %u of %u", (unsigned) gctl->help_topic,
            (unsigned) (AMIGA_HELP_TOPICS - 1));
    config_nio_set_status(gctl->state, tmp_text);
  }
  gctl->help.top = 0;
  gctl->help.selected = 0;
  amiga_list_set_count(&gctl->help, count);
}

static void gui_set_page(uint8_t page)
{
  if (gctl->page == AMIGA_PAGE_MOUNT && page != AMIGA_PAGE_MOUNT)
    amiga_ctl_mount_cancel(gctl);
  if (gctl->page == AMIGA_PAGE_HELP && page != AMIGA_PAGE_HELP)
    amiga_ctl_help_close(gctl);
  amiga_ctl_set_page(gctl, page);
  gui_sync_gadgets();
  gui_sync_editors();
  gui_paint();
}

/* Repaints after an action; a page change needs the full treatment. */
static void gui_after_action(uint8_t old_page)
{
  gui_help_sync();
  if (gctl->page != old_page) {
    gui_set_page(gctl->page);
    return;
  }
  gui_sync_editors();
  gui_paint_info();
  gui_paint_rows();
  gui_paint_buttons();
  gui_paint_status();
  update_prop();
}

/* ---- Actions ------------------------------------------------------------- */

/* Destructive actions ask first, before the window is marked busy. */
static int gui_confirm_action(uint8_t action)
{
  switch (action) {
  case ACT_HOST_REMOVE:
    return confirm("Remove this host?");
  case ACT_SLOT_CLEAR:
    return confirm("Clear this catalogue slot?");
  case ACT_DRIVE_EJECT:
    if (gctl->drives.selected == AMIGA_LIST_NONE)
      return 1;
    sprintf(tmp_text, "Eject %s?",
            amiga_drive_label((uint8_t) gctl->drives.selected, gctl->kick13));
    return confirm(tmp_text);
  case ACT_MOUNT_COMMIT:
    if (gctl->drives.selected == AMIGA_LIST_NONE ||
        !amiga_ctl_drive_mounted(gctl, (uint8_t) gctl->drives.selected))
      return 1;
    sprintf(tmp_text, "Replace the disk in %s\nwith %.40s?",
            amiga_drive_label((uint8_t) gctl->drives.selected, gctl->kick13),
            gctl->mount_name);
    return confirm(tmp_text);
  default:
    return 1;
  }
}

static int gui_touch_drive(const char *name);

static void gui_do_action(uint8_t action)
{
  uint8_t old_page = gctl->page;
  uint8_t slot;

  if (!gui_confirm_action(action))
    return;
  busy_begin();
  switch (action) {
  case ACT_HOST_BROWSE:
    (void) amiga_ctl_browse_open(gctl);
    break;
  case ACT_HOST_ADD:
    (void) amiga_ctl_host_add(gctl, (const char *) edit_buf);
    break;
  case ACT_HOST_REPLACE:
    (void) amiga_ctl_host_replace(gctl, (const char *) edit_buf);
    break;
  case ACT_HOST_REMOVE:
    (void) amiga_ctl_host_remove(gctl);
    break;
  case ACT_HOST_UP:
    (void) amiga_ctl_host_move(gctl, -1);
    break;
  case ACT_HOST_DOWN:
    (void) amiga_ctl_host_move(gctl, 1);
    break;
  case ACT_BROWSE_OPEN:
    (void) amiga_ctl_browse_activate(gctl);
    break;
  case ACT_BROWSE_PARENT:
    (void) amiga_ctl_browse_parent(gctl);
    break;
  case ACT_BROWSE_REFRESH:
    if (gctl->browse_open)
      (void) amiga_ctl_browse_refresh(gctl);
    else
      (void) amiga_ctl_browse_open(gctl);
    break;
  case ACT_BROWSE_MOUNT:
    if (amiga_ctl_mount_begin_browse(gctl))
      set_ro(1);
    break;
  case ACT_SLOT_SET:
    if (read_slot(&slot))
      (void) amiga_ctl_slot_set(gctl, slot, (const char *) edit_buf,
                                (uint8_t) read_ro());
    break;
  case ACT_SLOT_CLEAR:
    if (read_slot(&slot))
      (void) amiga_ctl_slot_clear(gctl, slot);
    break;
  case ACT_SLOT_MOUNT:
    if (read_slot(&slot) && amiga_ctl_mount_begin_slot(gctl, slot))
      set_ro(1);
    break;
  case ACT_MOUNT_COMMIT:
    if (gctl->drives.selected != AMIGA_LIST_NONE) {
      uint8_t unit = (uint8_t) gctl->drives.selected;

      /* Touch the drive so its disk icon appears, like a floppy. */
      if (amiga_ctl_mount_commit(gctl, unit, (uint8_t) read_ro()))
        (void) gui_touch_drive(amiga_drive_label(unit, gctl->kick13));
    }
    break;
  case ACT_MOUNT_CANCEL:
    amiga_ctl_mount_cancel(gctl);
    break;
  case ACT_HELP_CONTENTS:
    amiga_ctl_help_open(gctl, AMIGA_HELP_CONTENTS);
    break;
  case ACT_HELP_PREV:
    amiga_ctl_help_step(gctl, -1);
    break;
  case ACT_HELP_NEXT:
    amiga_ctl_help_step(gctl, 1);
    break;
  case ACT_HELP_CLOSE:
    amiga_ctl_help_close(gctl);
    break;
  case ACT_DRIVE_EJECT:
    if (gctl->drives.selected != AMIGA_LIST_NONE)
      (void) amiga_ctl_drive_eject(gctl, (uint8_t) gctl->drives.selected);
    break;
  case ACT_DRIVE_REMOUNT:
    if (gctl->drives.selected != AMIGA_LIST_NONE) {
      uint8_t unit = (uint8_t) gctl->drives.selected;

      if (amiga_ctl_drive_remount(gctl, unit))
        (void) gui_touch_drive(amiga_drive_label(unit, gctl->kick13));
    }
    break;
  default:
    break;
  }
  busy_end();
  gui_after_action(old_page);
}

/* A freshly mounted drive's filesystem only starts when something uses
 * the drive; until then Workbench has no volume or disk icon for it.
 * Locking the root starts it, as inserting a floppy would. */
static int gui_touch_drive(const char *name)
{
  BPTR lock = Lock((CONST_STRPTR) name, SHARED_LOCK);

  if (!lock)
    return 0;
  UnLock(lock);
  return 1;
}

/* Opens a mounted drive the way double-clicking its disk icon does.  Only
 * workbench.library V44 (Workbench 3.5) and later can be asked to. */
static void gui_open_drive_window(void)
{
  char name[8];

  if (gctl->drives.selected == AMIGA_LIST_NONE ||
      !amiga_ctl_drive_window_name(gctl, (uint8_t) gctl->drives.selected,
                                   name, sizeof(name))) {
    gui_paint_status();
    return;
  }
  strcpy(tmp_text, "Opening drive windows needs Workbench 3.5 or later");
#ifndef __KICK13__
  WorkbenchBase = OpenLibrary((CONST_STRPTR) "workbench.library", 44);
  if (WorkbenchBase) {
    int tries;
    int opened = 0;

    busy_begin();
    if (gui_touch_drive(name)) {
      /* Workbench notices a newly started volume on its own schedule
       * (measured at over 2 seconds on WB3.2); wait up to 10 seconds. */
      for (tries = 0; tries < 50 && !opened; tries++) {
        opened = OpenWorkbenchObjectA((CONST_STRPTR) name, NULL) != 0;
        if (!opened)
          Delay(10);
      }
    }
    busy_end();
    if (opened)
      sprintf(tmp_text, "Opened %s on Workbench", name);
    else
      sprintf(tmp_text, "Workbench could not open %s", name);
    CloseLibrary(WorkbenchBase);
    WorkbenchBase = NULL;
  }
#endif
  config_nio_set_status(gctl->state, tmp_text);
  gui_paint_status();
}

static void gui_open_help(uint8_t topic)
{
  uint8_t old_page = gctl->page;

  amiga_ctl_help_open(gctl, topic);
  gui_after_action(old_page);
}

/* Return / double-click: the page's primary action. */
static void gui_activate(void)
{
  switch (gctl->page) {
  case AMIGA_PAGE_HELP:
    if (gctl->help_topic == AMIGA_HELP_CONTENTS &&
        gctl->help.selected != AMIGA_LIST_NONE)
      gui_open_help((uint8_t) (gctl->help.selected + 1));
    break;
  case AMIGA_PAGE_HOSTS:
    gui_do_action(ACT_HOST_BROWSE);
    break;
  case AMIGA_PAGE_BROWSE: {
    amiga_list_t *l = &gctl->entries;

    /* Drawers open; image files go straight to the mount picker. */
    if (l->selected != AMIGA_LIST_NONE &&
        l->selected < gctl->state->entry_count &&
        !(gctl->state->entries[l->selected].is_dir & CONFIG_NIO_ENTRY_FLAG_DIR))
      gui_do_action(ACT_BROWSE_MOUNT);
    else
      gui_do_action(ACT_BROWSE_OPEN);
    break;
  }
  case AMIGA_PAGE_CATALOGUE:
    (void) ActivateGadget(&gad[GID_EDIT], win, NULL);
    break;
  case AMIGA_PAGE_MOUNT:
    gui_do_action(ACT_MOUNT_COMMIT);
    break;
  case AMIGA_PAGE_DRIVES:
    /* A saved drive that is not mounted yet is mounted again first. */
    if (gctl->drives.selected != AMIGA_LIST_NONE &&
        amiga_ctl_drive_state(gctl, (uint8_t) gctl->drives.selected) ==
          AMIGA_DRIVE_SAVED)
      gui_do_action(ACT_DRIVE_REMOUNT);
    else
      gui_open_drive_window();
    break;
  default:
    break;
  }
}

static void gui_select(uint16_t index)
{
  amiga_list_select(page_list(), index);
  gui_sync_editors();
  gui_paint_rows();
  if (gctl->page == AMIGA_PAGE_MOUNT)
    gui_paint_buttons();
  update_prop();
}

static void gui_list_click(struct IntuiMessage *msg)
{
  amiga_rect_t in = list_interior();
  uint16_t index;

  if (!amiga_list_hit(page_list(), (int16_t) (msg->MouseY - in.top),
                      layout.row_h, &index))
    return;
  if (index == last_index &&
      DoubleClick(last_secs, last_micros, msg->Seconds, msg->Micros)) {
    last_index = AMIGA_LIST_NONE;
    gui_select(index);
    gui_activate();
    return;
  }
  last_index = index;
  last_secs = msg->Seconds;
  last_micros = msg->Micros;
  gui_select(index);
}

/* The page button for the current page (Catalogue counts as Hosts). */
static uint8_t current_tab(void)
{
  uint8_t page = gctl->page == AMIGA_PAGE_HELP ? gctl->help_return
                                               : gctl->page;
  uint8_t i;

  if (page == AMIGA_PAGE_MOUNT)
    page = gctl->mount_return;

  for (i = 0; i < AMIGA_TAB_COUNT; i++) {
    if (tab_pages[i] == page)
      return i;
  }
  return 0;
}

/* Returns 1 when the user asked to quit. */
static int gui_handle_key(UWORD code, UWORD qualifier)
{
  amiga_list_t *l = page_list();
  amiga_key_t key = amiga_key_from_raw(code, qualifier);

  /* A help topic has no selection: the movement keys scroll the text. */
  if (gctl->page == AMIGA_PAGE_HELP &&
      gctl->help_topic != AMIGA_HELP_CONTENTS) {
    int32_t max_top = l->count > l->rows ? l->count - l->rows : 0;
    int32_t top = l->top;

    switch (key) {
    case AMIGA_KEY_UP: top -= 1; break;
    case AMIGA_KEY_DOWN: top += 1; break;
    case AMIGA_KEY_PAGE_UP: top -= l->rows; break;
    case AMIGA_KEY_PAGE_DOWN: top += l->rows; break;
    case AMIGA_KEY_TOP: top = 0; break;
    case AMIGA_KEY_BOTTOM: top = max_top; break;
    default: goto not_scroll;
    }
    l->top = (uint16_t) (top < 0 ? 0 : (top > max_top ? max_top : top));
    gui_paint_rows();
    update_prop();
    return 0;
  }
not_scroll:
  switch (key) {
  case AMIGA_KEY_UP:
    amiga_list_move(l, -1);
    break;
  case AMIGA_KEY_DOWN:
    amiga_list_move(l, 1);
    break;
  case AMIGA_KEY_PAGE_UP:
    amiga_list_move(l, (int16_t) -layout.list_rows);
    break;
  case AMIGA_KEY_PAGE_DOWN:
    amiga_list_move(l, (int16_t) layout.list_rows);
    break;
  case AMIGA_KEY_TOP:
    amiga_list_move(l, (int16_t) -l->count);
    break;
  case AMIGA_KEY_BOTTOM:
    amiga_list_move(l, (int16_t) l->count);
    break;
  case AMIGA_KEY_ACTIVATE:
    gui_activate();
    return 0;
  case AMIGA_KEY_PARENT:
    if (gctl->page == AMIGA_PAGE_BROWSE)
      gui_do_action(ACT_BROWSE_PARENT);
    return 0;
  case AMIGA_KEY_CANCEL:
    if (gctl->page == AMIGA_PAGE_MOUNT) {
      gui_do_action(ACT_MOUNT_CANCEL);
      return 0;
    }
    if (gctl->page == AMIGA_PAGE_HELP) {
      gui_do_action(ACT_HELP_CLOSE);
      return 0;
    }
    return 1;
  case AMIGA_KEY_HELP:
    gui_open_help(AMIGA_HELP_CONTENTS);
    return 0;
  case AMIGA_KEY_NEXT_PAGE:
    gui_set_page(tab_pages[(current_tab() + 1) % AMIGA_TAB_COUNT]);
    return 0;
  case AMIGA_KEY_PREV_PAGE:
    gui_set_page(tab_pages[(current_tab() + AMIGA_TAB_COUNT - 1) %
                           AMIGA_TAB_COUNT]);
    return 0;
  default:
    return 0;
  }
  gui_sync_editors();
  gui_paint_rows();
  if (gctl->page == AMIGA_PAGE_MOUNT)
    gui_paint_buttons();
  update_prop();
  return 0;
}

/* Returns 1 when the user asked to quit. */
static int gui_handle_gadget(struct Gadget *g, struct IntuiMessage *msg,
                             int down)
{
  UWORD id = g->GadgetID;

  if (id == GID_PROP) {
    prop_active = (uint8_t) down;
    amiga_list_set_top_from_pot(page_list(), prop_pi.VertPot);
    gui_paint_rows();
    return 0;
  }
  if (id == GID_LIST) {
    if (down)
      gui_list_click(msg);
    return 0;
  }
  if (down)
    return 0;
  if (id < GID_TAB0 + AMIGA_TAB_COUNT) {
    gui_set_page(tab_pages[id - GID_TAB0]);
  } else if (id >= GID_BTN0 && id < GID_COUNT) {
    uint8_t action = page_buttons[gctl->page][id - GID_BTN0].action;

    if (action != ACT_NONE)
      gui_do_action(action);
  } else if (id == GID_RO) {
    paint_checkbox();
  } else if (id == GID_SLOT && gctl->page == AMIGA_PAGE_CATALOGUE) {
    uint8_t slot;

    if (read_slot(&slot))
      gui_select(slot);
    else
      gui_paint_status();
  }
  return 0;
}

/* ---- Menus ------------------------------------------------------------- */

static void make_item(struct MenuItem *item, struct IntuiText *text,
                      const char *label, WORD top, WORD width, UWORD flags,
                      LONG exclude, BYTE key)
{
  make_itext(text, (WORD) ((flags & CHECKIT) ? CHECKWIDTH : 2), 1, label,
             NULL);
  text->FrontPen = 0;
  memset(item, 0, sizeof(*item));
  item->TopEdge = top;
  item->Width = width;
  item->Height = 10;
  item->Flags = (UWORD) (ITEMTEXT | ITEMENABLED | HIGHCOMP | flags);
  item->MutualExclude = exclude;
  item->ItemFill = (APTR) text;
  item->Command = key;
}

static void gui_make_menus(void)
{
  WORD w;
  uint8_t i;
  const config_nio_prefs_t *p = &gctl->state->prefs;

  w = (WORD) (12 * FONT_W + COMMWIDTH + 8);
  for (i = 0; i < 3; i++) {
    make_item(&project_items[i], &menu_text[i], project_labels[i],
              (WORD) (i * 10), w, COMMSEQ, 0, project_keys[i]);
    project_items[i].NextItem = i + 1 < 3 ? &project_items[i + 1] : NULL;
  }
  w = (WORD) (14 * FONT_W + CHECKWIDTH + 8);
  for (i = 0; i < 4; i++) {
    UWORD checked = 0;

    if ((i == 0 && p->date_format != CONFIG_NIO_PREF_DATE_YDM) ||
        (i == 1 && p->date_format == CONFIG_NIO_PREF_DATE_YDM) ||
        (i == 2 && p->size_format != CONFIG_NIO_PREF_SIZE_COMPACT) ||
        (i == 3 && p->size_format == CONFIG_NIO_PREF_SIZE_COMPACT))
      checked = CHECKED;
    make_item(&settings_items[i], &menu_text[3 + i], settings_labels[i],
              (WORD) (i * 10), w, (UWORD) (CHECKIT | checked),
              (LONG) (1L << (i ^ 1)), 0);
    settings_items[i].NextItem = i + 1 < 4 ? &settings_items[i + 1] : NULL;
  }
  /* Help menu: Contents (Right-Amiga-H), then every topic. */
  w = (WORD) (22 * FONT_W + COMMWIDTH + 8);
  for (i = 0; i < AMIGA_HELP_TOPICS; i++) {
    make_item(&help_items[i], &menu_text[7 + i], amiga_help_title(i),
              (WORD) (i * 10), w, (UWORD) (i == 0 ? COMMSEQ : 0), 0,
              (BYTE) (i == 0 ? 'H' : 0));
    help_items[i].NextItem = i + 1 < AMIGA_HELP_TOPICS ? &help_items[i + 1]
                                                       : NULL;
  }
  memset(menus, 0, sizeof(menus));
  menus[0].NextMenu = &menus[1];
  menus[0].LeftEdge = 0;
  menus[0].Width = 8 * FONT_W;
  menus[0].Height = 10;
  menus[0].Flags = MENUENABLED;
  menus[0].MenuName = (APTR) "Project";
  menus[0].FirstItem = &project_items[0];
  menus[1].LeftEdge = 9 * FONT_W;
  menus[1].Width = 9 * FONT_W;
  menus[1].Height = 10;
  menus[1].Flags = MENUENABLED;
  menus[1].MenuName = (APTR) "Settings";
  menus[1].FirstItem = &settings_items[0];
  menus[1].NextMenu = &menus[2];
  menus[2].LeftEdge = 19 * FONT_W;
  menus[2].Width = 5 * FONT_W;
  menus[2].Height = 10;
  menus[2].Flags = MENUENABLED;
  menus[2].MenuName = (APTR) "Help";
  menus[2].FirstItem = &help_items[0];
  menus_attached = SetMenuStrip(win, &menus[0]) ? 1 : 0;
}

/* Returns 1 when the user asked to quit. */
static int gui_handle_menu(UWORD code)
{
  int quit = 0;
  int settings = 0;

  while (code != MENUNULL) {
    struct MenuItem *item = ItemAddress(&menus[0], code);

    if (!item)
      break;
    if (MENUNUM(code) == 0) {
      if (ITEMNUM(code) == 0)
        gui_set_page(AMIGA_PAGE_CATALOGUE);
      else if (ITEMNUM(code) == 1)
        about();
      else
        quit = 1;
    } else if (MENUNUM(code) == 1) {
      settings = 1;
    } else if (MENUNUM(code) == 2) {
      gui_open_help((uint8_t) ITEMNUM(code));
    }
    code = item->NextSelect;
  }
  if (settings) {
    uint8_t date = (settings_items[1].Flags & CHECKED)
                   ? CONFIG_NIO_PREF_DATE_YDM : CONFIG_NIO_PREF_DATE_YMD;
    uint8_t size = (settings_items[3].Flags & CHECKED)
                   ? CONFIG_NIO_PREF_SIZE_COMPACT : CONFIG_NIO_PREF_SIZE_FULL;

    busy_begin();
    (void) amiga_ctl_set_prefs(gctl, date, size);
    busy_end();
    gui_paint_rows();
    gui_paint_status();
  }
  return quit;
}

/* ---- Script mode ----------------------------------------------------------- */

static void script_out(const char *line, void *ctx)
{
  fprintf((FILE *) ctx, "%s\n", line);
}

static int gui_run_script(const amiga_options_t *opts)
{
  static char line[CONFIG_NIO_URI_MAX + 64];
  FILE *in;
  FILE *out;
  unsigned ok = 0;
  unsigned err = 0;

  in = fopen(opts->script, "r");
  out = fopen(opts->result, "w");
  if (!in || !out) {
    if (in)
      fclose(in);
    if (out)
      fclose(out);
    amiga_gui_fatal("Unable to open the SCRIPT or RESULT file.");
    return 20;
  }
  while (fgets(line, sizeof(line), in)) {
    uint16_t ticks = 0;
    char *nl = strchr(line, '\n');
    int rc;

    if (nl)
      *nl = 0;
    fprintf(out, "> %s\n", line);
    busy_begin();
    rc = amiga_script_line(gctl, line, script_out, out, &ticks);
    busy_end();
    gui_set_page(gctl->page);
    if (rc == AMIGA_SCRIPT_QUIT)
      break;
    if (rc == AMIGA_SCRIPT_WAIT)
      Delay(ticks);
    else if (rc == AMIGA_SCRIPT_ERR)
      err++;
    else if (line[0] && line[0] != ';')
      ok++;
  }
  fprintf(out, "SCRIPT DONE ok=%u err=%u\n", ok, err);
  fclose(out);
  fclose(in);
  return err ? 5 : 0;
}

/* ---- Window lifecycle ------------------------------------------------------ */

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
  in.logo_w = AMIGA_LOGO_W;
  in.logo_h = AMIGA_LOGO_H;
  *ok = amiga_layout_compute(&in, &layout);
}

static void load_theme(void)
{
  amiga_theme_classic(&theme);
  new_look = 0;
#ifndef __KICK13__
  if (IntuitionBase->LibNode.lib_Version >= 36) {
    struct DrawInfo *dri = GetScreenDrawInfo(win->WScreen);

    new_look = 1;
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
  logo_chip = (UWORD *) AllocMem(2 * LOGO_MASK_BYTES, MEMF_CHIP);
  if (logo_chip) {
    CopyMem((APTR) amiga_logo_emblem, logo_chip, LOGO_MASK_BYTES);
    CopyMem((APTR) amiga_logo_text, (UBYTE *) logo_chip + LOGO_MASK_BYTES,
            LOGO_MASK_BYTES);
  }
  busy_sprite = (UWORD *) AllocMem(sizeof(busy_image), MEMF_CHIP);
  if (busy_sprite)
    CopyMem((APTR) busy_image, busy_sprite, sizeof(busy_image));
  amiga_ctl_set_rows(gctl, layout.list_rows);
  gui_make_gadgets();
  gui_make_menus();
  gui_sync_gadgets();
  return 1;
}

static void gui_close(void)
{
  uint8_t id;

  if (win) {
    if (menus_attached)
      ClearMenuStrip(win);
    menus_attached = 0;
    for (id = 0; id < GID_COUNT; id++)
      detach(id);
    CloseWindow(win);
    win = NULL;
  }
  if (busy_sprite) {
    FreeMem(busy_sprite, sizeof(busy_image));
    busy_sprite = NULL;
  }
  if (logo_chip) {
    FreeMem(logo_chip, 2 * LOGO_MASK_BYTES);
    logo_chip = NULL;
  }
  if (font) {
    CloseFont(font);
    font = NULL;
  }
}

int amiga_gui_run(amiga_ctl_t *ctl, const amiga_options_t *opts)
{
  struct IntuiMessage *msg;
  int done = 0;
  int rc = 0;

  gctl = ctl;
  if (!gui_open()) {
    gui_close();
    return 20;
  }
  gui_sync_editors();
  gui_paint();

  if (opts->script[0]) {
    rc = gui_run_script(opts);
    gui_close();
    return rc;
  }

  while (!done) {
    WaitPort(win->UserPort);
    while ((msg = (struct IntuiMessage *) GetMsg(win->UserPort)) != NULL) {
      ULONG cls = msg->Class;
      UWORD code = msg->Code;
      UWORD qual = msg->Qualifier;
      struct Gadget *g = (struct Gadget *) msg->IAddress;
      struct IntuiMessage copy = *msg;

      ReplyMsg((struct Message *) msg);
      if (cls == CLOSEWINDOW)
        done = 1;
      else if (cls == GADGETDOWN)
        done |= gui_handle_gadget(g, &copy, 1);
      else if (cls == GADGETUP)
        done |= gui_handle_gadget(g, &copy, 0);
      else if (cls == MOUSEMOVE && prop_active) {
        amiga_list_set_top_from_pot(page_list(), prop_pi.VertPot);
        gui_paint_rows();
      } else if (cls == RAWKEY)
        done |= gui_handle_key(code, qual);
      else if (cls == MENUPICK)
        done |= gui_handle_menu(code);
    }
  }
  gui_close();
  return rc;
}
