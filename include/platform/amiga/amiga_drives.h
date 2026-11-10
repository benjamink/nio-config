#ifndef AMIGA_DRIVES_H
#define AMIGA_DRIVES_H

#include <stdint.h>

#define AMIGA_DRIVE_COUNT 8
/* Resolve these through the launching process's AmigaDOS command path. */
#define AMIGA_DEFAULT_FMOUNT "fmount"
#define AMIGA_DEFAULT_FUMOUNT "fumount"

/* Media class of an ADF as far as drive choice goes.  known == 0 means it
 * could not be inspected (older firmware, unreadable image): no drive is then
 * excluded. */
typedef struct {
  uint8_t known;
  uint8_t hd;    /* high density (1760 KiB) rather than double density */
  uint8_t ofs;   /* DOS\0 rather than DOS\1 (FFS) */
} amiga_media_t;

enum {
  AMIGA_FIT_OK = 0,
  AMIGA_FIT_DENSITY,   /* wrong density for the drive */
  AMIGA_FIT_FS         /* wrong filesystem for the drive */
};

/* Classify from a DiskDevice inspection: geometry plus the boot block's
 * DosType longword.  Anything but a recognised ADF leaves known == 0. */
void amiga_media_classify(amiga_media_t *media, uint16_t sector_size,
                          uint32_t sector_count, const uint8_t *boot,
                          uint16_t boot_len);
/* "DD FFS" etc., or "?" when unknown. */
const char *amiga_media_name(const amiga_media_t *media);

/* Whether `media` may go on `unit`.  `pinned` is what the unit's running
 * handler is fixed to (WB3.1 FFS cannot change geometry or filesystem),
 * or NULL/unknown when it is free to change.  On WB1.3 the endpoint itself
 * fixes the category, so `pinned` is ignored.  Returns AMIGA_FIT_*. */
uint8_t amiga_drive_fit(uint8_t unit, uint8_t kick13,
                        const amiga_media_t *media,
                        const amiga_media_t *pinned);
/* The media category a WB1.3 endpoint is fixed to. */
amiga_media_t amiga_drive_kick13_media(uint8_t unit);

const char *amiga_drive_label(uint8_t unit, uint8_t kick13);
int amiga_drive_unit(const char *label, uint8_t kick13);
int amiga_fmount_command(char *out, uint16_t cap, const char *fmount,
                         uint8_t slot, uint8_t unit, uint8_t kick13,
                         uint8_t readonly);
int amiga_fumount_command(char *out, uint16_t cap, const char *fumount,
                          uint8_t unit, uint8_t kick13);

#endif
