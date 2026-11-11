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

uint8_t amiga_handler_pins_media(uint16_t dos_version)
{
  return dos_version == 40;
}

amiga_media_t amiga_drive_kick13_media(uint8_t unit)
{
  amiga_media_t m;

  /* Units 0-1 DD/FFS, 2-3 HD/FFS, 4-5 DD/OFS, 6-7 HD/OFS. */
  m.known = 1;
  m.hd = (uint8_t) ((unit >> 1) & 1);
  m.ofs = (uint8_t) ((unit >> 2) & 1);
  return m;
}

void amiga_media_classify(amiga_media_t *media, uint16_t sector_size,
                          uint32_t sector_count, const uint8_t *boot,
                          uint16_t boot_len)
{
  uint32_t bytes;

  media->known = media->hd = media->ofs = 0;
  if (sector_size != 512 || !boot || boot_len < 4 || boot[0] != 'D' ||
      boot[1] != 'O' || boot[2] != 'S' || boot[3] > 1)
    return;
  bytes = sector_count;            /* 512-byte sectors: 1760 DD, 3520 HD */
  if (bytes == 1760UL)
    media->hd = 0;
  else if (bytes == 3520UL)
    media->hd = 1;
  else
    return;
  media->ofs = (uint8_t) (boot[3] == 0);
  media->known = 1;
}

const char *amiga_media_name(const amiga_media_t *media)
{
  if (!media || !media->known)
    return "?";
  if (media->hd)
    return media->ofs ? "HD OFS" : "HD FFS";
  return media->ofs ? "DD OFS" : "DD FFS";
}

uint8_t amiga_drive_fit(uint8_t unit, uint8_t kick13,
                        const amiga_media_t *media,
                        const amiga_media_t *pinned)
{
  amiga_media_t fixed;

  if (!media || !media->known)
    return AMIGA_FIT_OK;
  if (kick13) {
    fixed = amiga_drive_kick13_media(unit);
    pinned = &fixed;
  }
  if (!pinned || !pinned->known)
    return AMIGA_FIT_OK;
  if (pinned->hd != media->hd)
    return AMIGA_FIT_DENSITY;
  if (pinned->ofs != media->ofs)
    return AMIGA_FIT_FS;
  return AMIGA_FIT_OK;
}

uint8_t amiga_fmount_flavour(const char *usage_line)
{
  if (!usage_line || !strstr(usage_line, "FMOUNT"))
    return AMIGA_TOOLS_UNKNOWN;
  if (strstr(usage_line, "HN0:"))
    return AMIGA_TOOLS_KICK13;
  if (strstr(usage_line, "DN0:"))
    return AMIGA_TOOLS_STANDARD;
  return AMIGA_TOOLS_UNKNOWN;
}

int amiga_tools_mismatch(uint8_t kick13, uint8_t flavour, char *msg,
                         uint16_t cap)
{
  const char *text;

  if (kick13 && flavour == AMIGA_TOOLS_STANDARD)
    text = "This is Workbench 1.3, which needs the Workbench 1.3 FMOUNT, "
           "FUMOUNT and fujinet-disk.device, but the standard FMOUNT is "
           "installed.\nInstall them from the Workbench 1.3 release disk "
           "(NIORelease-WB13).";
  else if (!kick13 && flavour == AMIGA_TOOLS_KICK13)
    text = "This is Workbench 2.0 or newer, which uses drives DN0: to DN7:, "
           "but the installed FMOUNT is the Workbench 1.3 build.\nInstall "
           "FMOUNT, FUMOUNT and fujinet-disk.device from the Workbench 3.x "
           "release disk (NIORelease-WB31 or NIORelease-WB32).";
  else
    return 0;
  if (msg && cap) {
    strncpy(msg, text, cap - 1);
    msg[cap - 1] = 0;
  }
  return 1;
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
