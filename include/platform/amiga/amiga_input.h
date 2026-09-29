#ifndef AMIGA_INPUT_H
#define AMIGA_INPUT_H

#include <stdint.h>

typedef enum {
  AMIGA_KEY_NONE = 0,
  AMIGA_KEY_UP,
  AMIGA_KEY_DOWN,
  AMIGA_KEY_PAGE_UP,
  AMIGA_KEY_PAGE_DOWN,
  AMIGA_KEY_TOP,
  AMIGA_KEY_BOTTOM,
  AMIGA_KEY_ACTIVATE,
  AMIGA_KEY_PARENT,
  AMIGA_KEY_CANCEL,
  AMIGA_KEY_HELP,
  AMIGA_KEY_NEXT_PAGE,
  AMIGA_KEY_PREV_PAGE
} amiga_key_t;

amiga_key_t amiga_key_from_raw(uint16_t code, uint16_t qualifier);

#endif
