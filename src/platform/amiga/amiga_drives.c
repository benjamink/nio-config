#include "amiga_drives.h"
#include "amiga_fmt.h"

#include <stdio.h>
#include <string.h>

static const char *const labels_v36[AMIGA_DRIVE_COUNT] = {
  "DN0:", "DN1:", "DN2:", "DN3:", "DN4:", "DN5:", "DN6:", "DN7:"
};

/* WB1.3 static MountList endpoints; the index is the fujinet-disk.device
 * unit (docs/amiga/disk-media-architecture.md, WB1.3 static-unit workflow). */
static const char *const labels_kick13[AMIGA_DRIVE_COUNT] = {
  "DN0:", "DN1:", "HN0:", "HN1:", "DO0:", "DO1:", "HO0:", "HO1:"
};

const char *amiga_drive_label(uint8_t unit, uint8_t kick13)
{
  if (unit >= AMIGA_DRIVE_COUNT)
    return NULL;
  return kick13 ? labels_kick13[unit] : labels_v36[unit];
}

static char upper(char c)
{
  return (char) ((c >= 'a' && c <= 'z') ? c - 32 : c);
}

int amiga_drive_unit(const char *label, uint8_t kick13)
{
  uint8_t unit;

  if (!label || !label[0])
    return -1;
  for (unit = 0; unit < AMIGA_DRIVE_COUNT; unit++) {
    const char *want;
    uint8_t i;

    want = amiga_drive_label(unit, kick13);
    i = 0;
    while (want[i] != ':' && upper(label[i]) == want[i])
      i++;
    if (want[i] == ':' &&
        (label[i] == 0 || (label[i] == ':' && label[i + 1] == 0)))
      return unit;
  }
  return -1;
}

static int path_ok(const char *path, uint16_t cap, uint16_t tail)
{
  const char *p;

  if (!path || !path[0])
    return 0;
  for (p = path; *p; p++) {
    if (*p == ' ' || *p == '"' || *p == '\n')
      return 0;
  }
  return (uint16_t) (strlen(path) + tail) < cap;
}

int amiga_fmount_command(char *out, uint16_t cap, const char *fmount,
                         uint8_t slot, uint8_t unit, uint8_t kick13,
                         uint8_t readonly)
{
  const char *label;

  label = amiga_drive_label(unit, kick13);
  /* " 255 DN0: RO" is at most 12 characters. */
  if (!out || !label || !path_ok(fmount, cap, 12))
    return 0;
  amiga_sprintf(out, "%s %u %s %s", fmount, (unsigned) slot, label,
          readonly ? "RO" : "RW");
  return 1;
}

int amiga_fumount_command(char *out, uint16_t cap, const char *fumount,
                          uint8_t unit, uint8_t kick13)
{
  const char *label;

  label = amiga_drive_label(unit, kick13);
  if (!out || !label || !path_ok(fumount, cap, 5))
    return 0;
  if (kick13)
    amiga_sprintf(out, "%s %s", fumount, label);
  else
    amiga_sprintf(out, "%s %u", fumount, (unsigned) unit);
  return 1;
}
