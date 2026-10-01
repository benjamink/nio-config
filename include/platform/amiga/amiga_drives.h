#ifndef AMIGA_DRIVES_H
#define AMIGA_DRIVES_H

#include <stdint.h>

#define AMIGA_DRIVE_COUNT 8
/* Resolve these through the launching process's AmigaDOS command path. */
#define AMIGA_DEFAULT_FMOUNT "fmount"
#define AMIGA_DEFAULT_FUMOUNT "fumount"

const char *amiga_drive_label(uint8_t unit, uint8_t kick13);
int amiga_drive_unit(const char *label, uint8_t kick13);
int amiga_fmount_command(char *out, uint16_t cap, const char *fmount,
                         uint8_t slot, uint8_t unit, uint8_t kick13,
                         uint8_t readonly);
int amiga_fumount_command(char *out, uint16_t cap, const char *fumount,
                          uint8_t unit, uint8_t kick13);

#endif
