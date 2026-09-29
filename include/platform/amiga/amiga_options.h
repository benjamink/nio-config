#ifndef AMIGA_OPTIONS_H
#define AMIGA_OPTIONS_H

#include <stdint.h>

#define AMIGA_OPT_PATH_MAX 96
#define AMIGA_DEFAULT_RESULT "RAM:config-nio.result"

/* Options come from Shell arguments or Workbench ToolTypes, both KEY=value. */
typedef struct {
  char fmount[AMIGA_OPT_PATH_MAX];
  char fumount[AMIGA_OPT_PATH_MAX];
  char script[AMIGA_OPT_PATH_MAX];
  char result[AMIGA_OPT_PATH_MAX];
} amiga_options_t;

void amiga_options_defaults(amiga_options_t *o);
/* 1 = applied, 0 = known key with an invalid value, -1 = unknown/disabled. */
int amiga_options_parse(amiga_options_t *o, const char *arg);
void amiga_options_finish(amiga_options_t *o);

#endif
