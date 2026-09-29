#ifndef AMIGA_HELP_H
#define AMIGA_HELP_H

#include <stdint.h>

/* Built-in help: a topic table and a word-wrapping layout, used by the
 * Help menu.  AmigaGuide is not part of Workbench 1.3, so the text is
 * shown in the main window instead. */
#define AMIGA_HELP_CONTENTS 0
#define AMIGA_HELP_TOPICS 12

typedef struct {
  uint16_t start;   /* offset into the topic text */
  uint8_t len;
  uint8_t indent;   /* leading spaces to draw (bullet continuation) */
} amiga_help_line_t;

const char *amiga_help_title(uint8_t topic);   /* out of range -> Contents */
const char *amiga_help_text(uint8_t topic);
uint8_t amiga_help_find(const char *title);     /* unknown -> Contents */

/* Wraps `text` to `cols` columns: '\n' ends a paragraph, lines break at
 * spaces, over-long words split, "- " bullets get a two-space hanging
 * indent.  Returns the number of lines written (at most `max`). */
uint16_t amiga_help_layout(const char *text, uint8_t cols,
                           amiga_help_line_t *lines, uint16_t max);

#endif
