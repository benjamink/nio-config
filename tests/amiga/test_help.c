#include "check.h"

#include <string.h>
#include "amiga_help.h"

#define MAX_LINES 400

static amiga_help_line_t lines[MAX_LINES];

static void line_text(const char *text, const amiga_help_line_t *l, char *out)
{
  memset(out, ' ', l->indent);
  memcpy(out + l->indent, text + l->start, l->len);
  out[l->indent + l->len] = 0;
}

void test_help(void)
{
  static const char sample[] =
    "Mounting an image is quick.\n"
    "\n"
    "- Double-click an image to choose a drive for it.\n"
    "Averyveryverylongwordthatcannotfit";
  char buf[128];
  uint16_t n;
  uint8_t t;

  n = amiga_help_layout(sample, 20, lines, MAX_LINES);
  CHECK(n == 8);
  line_text(sample, &lines[0], buf);
  CHECK_STR(buf, "Mounting an image is");
  line_text(sample, &lines[1], buf);
  CHECK_STR(buf, "quick.");
  line_text(sample, &lines[2], buf);
  CHECK_STR(buf, "");
  line_text(sample, &lines[3], buf);
  CHECK_STR(buf, "- Double-click an");
  line_text(sample, &lines[4], buf);
  CHECK_STR(buf, "  image to choose a");   /* bullets get a hanging indent */
  line_text(sample, &lines[5], buf);
  CHECK_STR(buf, "  drive for it.");
  line_text(sample, &lines[6], buf);
  CHECK_STR(buf, "Averyveryverylongwor");  /* words longer than a line split */
  line_text(sample, &lines[7], buf);
  CHECK_STR(buf, "dthatcannotfit");

  /* The output never overflows the caller's table. */
  CHECK(amiga_help_layout(sample, 20, lines, 3) == 3);

  /* Topic table: Contents first, every topic titled, non-empty and able to
   * lay out at the window's width without truncation. */
  CHECK(AMIGA_HELP_TOPICS >= 10);
  CHECK_STR(amiga_help_title(AMIGA_HELP_CONTENTS), "Contents");
  for (t = 0; t < AMIGA_HELP_TOPICS; t++) {
    const char *text = amiga_help_text(t);
    uint16_t i;

    CHECK(amiga_help_title(t)[0] != 0);
    CHECK(text && text[0]);
    n = amiga_help_layout(text, 72, lines, MAX_LINES);
    CHECK(n > 0 && n < MAX_LINES);
    for (i = 0; i < n; i++)
      CHECK(lines[i].indent + lines[i].len <= 72);
  }
  CHECK(amiga_help_title(AMIGA_HELP_TOPICS) == amiga_help_title(0));
  CHECK(amiga_help_find("Starting automatically") != 0);
  CHECK(amiga_help_find("Favorites") == AMIGA_HELP_CONTENTS);
  CHECK(amiga_help_find("Catalogue") != 0);
  /* Each GUI action names the Shell command it matches. */
  CHECK(strstr(amiga_help_text(amiga_help_find("Browsing and mounting")),
               "FIN then FMOUNT") != NULL);
  CHECK(strstr(amiga_help_text(amiga_help_find("Catalogue")), "FOUT") != NULL);
  CHECK(strstr(amiga_help_text(amiga_help_find("Drives and ejecting")),
               "FUMOUNT") != NULL);
  CHECK(amiga_help_find("No such topic") == AMIGA_HELP_CONTENTS);
}
