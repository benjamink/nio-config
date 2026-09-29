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

int amiga_digit_from_raw(uint16_t code)
{
  if (code & RAW_KEY_UP)
    return -1;
  if (code >= 0x01 && code <= 0x09)        /* 1-9 on the main row */
    return (int) code;
  switch (code) {
  case 0x0A: case 0x0F: return 0;          /* 0, keypad 0 */
  case 0x1D: return 1;
  case 0x1E: return 2;
  case 0x1F: return 3;
  case 0x2D: return 4;
  case 0x2E: return 5;
  case 0x2F: return 6;
  case 0x3D: return 7;
  case 0x3E: return 8;
  case 0x3F: return 9;
  default: return -1;
  }
}

void amiga_typeahead_reset(amiga_typeahead_t *t)
{
  t->value = 0;
  t->active = 0;
  t->last_ms = 0;
}

uint16_t amiga_typeahead_feed(amiga_typeahead_t *t, uint8_t digit,
                              uint32_t now_ms, uint16_t max)
{
  uint32_t v = (uint32_t) t->value * 10u + digit;

  if (!t->active || (uint32_t) (now_ms - t->last_ms) > AMIGA_TYPEAHEAD_MS ||
      v > max)
    v = digit;
  t->value = (uint16_t) v;
  t->active = 1;
  t->last_ms = now_ms;
  return t->value;
}
