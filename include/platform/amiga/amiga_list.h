#ifndef AMIGA_LIST_H
#define AMIGA_LIST_H

#include <stdint.h>

#define AMIGA_LIST_NONE 0xFFFFu

/* Selection and scroll state for one listview.  Scrolling with the
 * proportional gadget moves `top` only; selection changes keep the selected
 * row visible. */
typedef struct {
  uint16_t count;
  uint16_t top;
  uint16_t selected;
  uint8_t rows;
} amiga_list_t;

void amiga_list_init(amiga_list_t *l, uint8_t rows);
void amiga_list_set_rows(amiga_list_t *l, uint8_t rows);
void amiga_list_set_count(amiga_list_t *l, uint16_t count);
void amiga_list_select(amiga_list_t *l, uint16_t index);
void amiga_list_move(amiga_list_t *l, int16_t delta);
int amiga_list_hit(const amiga_list_t *l, int16_t y, uint8_t row_h,
                   uint16_t *index);
void amiga_list_prop(const amiga_list_t *l, uint16_t *pot, uint16_t *body);
void amiga_list_set_top_from_pot(amiga_list_t *l, uint16_t pot);

#endif
