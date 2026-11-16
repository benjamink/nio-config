#ifndef TRS80_UI_WIDGETS_H
#define TRS80_UI_WIDGETS_H

#include <stdint.h>

/* Text widgets over the con_* console. Selection is marked with '>' (the 4P has no usable
 * reverse video), so no widget depends on attributes. */

/* ---- list ---- */
typedef const char *(*list_item_fn)(uint16_t index, void *ctx);

typedef struct {
  uint8_t row, col, width, height;   /* screen rectangle */
  uint16_t count;                    /* items known */
  uint16_t sel, top;                 /* selected index, first visible index */
  list_item_fn item;
  void *ctx;
} list_t;

enum { LIST_NONE = 0, LIST_MOVED = 1, LIST_EDGE_UP = 2, LIST_EDGE_DOWN = 3 };

void list_draw(const list_t *l);
/* Up/Down move one, SHIFT+Up/Down move height-1 (clamped). Returns LIST_MOVED when the
 * selection changed, LIST_EDGE_UP / LIST_EDGE_DOWN for Up on the first / Down on the last
 * item (the caller may then load the previous / next page), LIST_NONE for any other key. */
int list_key(list_t *l, int key);

/* ---- text field ---- */
/* Edits buf (room for max+1 bytes) at row/col, shown through a `width` window that scrolls to
 * the end. ENTER accepts (returns 1); BREAK restores the original text and returns 0.
 * Left (Backspace) deletes the last character; SHIFT+Left clears the field. Printable ASCII
 * is inserted at the end. max must be <= 255. */
int field_edit(char *buf, uint8_t max, uint8_t row, uint8_t col, uint8_t width);

/* ---- tabs and messages ---- */
void tabs_draw(uint8_t row, const char *const *names, uint8_t n, uint8_t active);
void msg_draw(uint8_t row, const char *text);

#endif
