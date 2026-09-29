#include "amiga_input.h"

#define RAW_UP 0x4C
#define RAW_DOWN 0x4D
#define RAW_RETURN 0x44
#define RAW_ENTER 0x43
#define RAW_ESC 0x45
#define RAW_HELP 0x5F
#define RAW_BACKSPACE 0x41
#define RAW_TAB 0x42
#define RAW_KEY_UP 0x80
#define QUAL_SHIFT 0x0003
#define QUAL_ALT 0x0030

amiga_key_t amiga_key_from_raw(uint16_t code, uint16_t qualifier)
{
  if (code & RAW_KEY_UP)
    return AMIGA_KEY_NONE;
  switch (code) {
  case RAW_UP:
    if (qualifier & QUAL_ALT)
      return AMIGA_KEY_TOP;
    return (qualifier & QUAL_SHIFT) ? AMIGA_KEY_PAGE_UP : AMIGA_KEY_UP;
  case RAW_DOWN:
    if (qualifier & QUAL_ALT)
      return AMIGA_KEY_BOTTOM;
    return (qualifier & QUAL_SHIFT) ? AMIGA_KEY_PAGE_DOWN : AMIGA_KEY_DOWN;
  case RAW_RETURN:
  case RAW_ENTER:
    return AMIGA_KEY_ACTIVATE;
  case RAW_BACKSPACE:
    return AMIGA_KEY_PARENT;
  case RAW_ESC:
    return AMIGA_KEY_CANCEL;
  case RAW_HELP:
    return AMIGA_KEY_HELP;
  case RAW_TAB:
    return (qualifier & QUAL_SHIFT) ? AMIGA_KEY_PREV_PAGE : AMIGA_KEY_NEXT_PAGE;
  default:
    return AMIGA_KEY_NONE;
  }
}
