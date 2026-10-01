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

/* 0-9 for a digit key on the main row or keypad, else -1. */
int amiga_digit_from_raw(uint16_t code);

/* Typing a number into a list: digits within AMIGA_TYPEAHEAD_MS of each
 * other build one number; a pause, or a digit that would pass `max`,
 * starts a new one.  Times are in milliseconds and may wrap. */
#define AMIGA_TYPEAHEAD_MS 2000UL

typedef struct {
  uint16_t value;
  uint8_t active;
  uint32_t last_ms;
} amiga_typeahead_t;

void amiga_typeahead_reset(amiga_typeahead_t *t);
uint16_t amiga_typeahead_feed(amiga_typeahead_t *t, uint8_t digit,
                              uint32_t now_ms, uint16_t max);

#endif
