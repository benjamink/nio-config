#include "amiga_options.h"
#include "amiga_drives.h"

#include <string.h>

static int key_is(const char *arg, uint16_t len, const char *key)
{
  uint16_t i;

  if (strlen(key) != len)
    return 0;
  for (i = 0; i < len; i++) {
    char c = arg[i];
    if (c >= 'a' && c <= 'z')
      c = (char) (c - 32);
    if (c != key[i])
      return 0;
  }
  return 1;
}

void amiga_options_defaults(amiga_options_t *o)
{
  memset(o, 0, sizeof(*o));
  strcpy(o->fmount, AMIGA_DEFAULT_FMOUNT);
  strcpy(o->fumount, AMIGA_DEFAULT_FUMOUNT);
}

int amiga_options_parse(amiga_options_t *o, const char *arg)
{
  const char *eq;
  const char *value;
  char *dst;
  uint16_t key_len;

  /* "(KEY=value)" is a disabled ToolType by Workbench convention. */
  if (!o || !arg || arg[0] == '(')
    return -1;
  eq = strchr(arg, '=');
  if (!eq)
    return -1;
  key_len = (uint16_t) (eq - arg);
  if (key_is(arg, key_len, "FMOUNT"))
    dst = o->fmount;
  else if (key_is(arg, key_len, "FUMOUNT"))
    dst = o->fumount;
  else if (key_is(arg, key_len, "SCRIPT"))
    dst = o->script;
  else if (key_is(arg, key_len, "RESULT"))
    dst = o->result;
  else
    return -1;
  value = eq + 1;
  if (!value[0] || strlen(value) >= AMIGA_OPT_PATH_MAX)
    return 0;
  strcpy(dst, value);
  return 1;
}

void amiga_options_finish(amiga_options_t *o)
{
  if (o->script[0] && !o->result[0])
    strcpy(o->result, AMIGA_DEFAULT_RESULT);
}
