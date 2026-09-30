#include "config_nio.h"
#include "fujinet-nio.h"

#include <string.h>

/* A short, specific reason for the last failed mount. */
static const char *mount_failure(void)
{
  switch (fnsvc_disk_last_error()) {
  case FN_DISK_ERR_GEOMETRY_REQUIRED: return "Mount: sector size needed";
  case FN_DISK_ERR_INVALID_GEOMETRY:  return "Mount: wrong sector size";
  case FN_DISK_ERR_FILE_NOT_FOUND:    return "Mount: image not found";
  case FN_DISK_ERR_BAD_IMAGE:         return "Mount: bad image";
  case FN_DISK_ERR_UNSUPPORTED_TYPE:  return "Mount: unsupported type";
  default:                            return "Mount failed";
  }
}

/* Sector size NIO should use for a mapped image whose geometry it cannot
 * work out from content or an unambiguous extension. 512 suits the raw
 * media of CONFNIO's current machines; a platform whose raw media differs
 * (Apple II 256, Atari 128/256) should pass its own. */
#define CONFIG_NIO_SECTOR_SIZE_HINT 512

static int has_scheme_or_prefix(const char *s)
{
  const char *p;

  p = s;
  while (*p && *p != '/' && *p != '\\') {
    if (*p == ':')
      return 1;
    p++;
  }
  return 0;
}

static int append_text(char *out, uint16_t cap, uint16_t *pos, const char *s)
{
  uint16_t len;

  len = (uint16_t) strlen(s);
  if ((uint16_t) (*pos + len) >= cap)
    return 0;
  memcpy(&out[*pos], s, len);
  *pos = (uint16_t) (*pos + len);
  out[*pos] = 0;
  return 1;
}

int config_nio_set_status(config_nio_state_t *state, const char *msg)
{
  uint16_t i;
  uint16_t cap;

  if (!state)
    return 0;
  if (!msg)
    msg = "";
  cap = (uint16_t) sizeof(state->status);
  for (i = 0; (uint16_t) (i + 1) < cap && msg[i]; i++)
    state->status[i] = msg[i];
  state->status[i] = 0;
  return 1;
}

int config_nio_compose_uri(const char *host, const char *path,
                           const char *leaf, char *out, uint16_t cap)
{
  uint16_t pos;
  uint16_t host_len;
  int needs_slash;

  if (!host || !*host || !out || cap == 0)
    return 0;

  out[0] = 0;
  pos = 0;

  if (!has_scheme_or_prefix(host)) {
    if (!append_text(out, cap, &pos, "tnfs://"))
      return 0;
  }

  if (!append_text(out, cap, &pos, host))
    return 0;

  host_len = (uint16_t) strlen(host);
  needs_slash = host_len == 0 || host[host_len - 1] != '/';
  if (path && *path) {
    if (needs_slash && !append_text(out, cap, &pos, "/"))
      return 0;
    if (!append_text(out, cap, &pos, path))
      return 0;
  } else if (leaf && *leaf) {
    if (needs_slash && !append_text(out, cap, &pos, "/"))
      return 0;
  } else if (!has_scheme_or_prefix(host) && needs_slash) {
    if (!append_text(out, cap, &pos, "/"))
      return 0;
  }

  if (leaf && *leaf) {
    if (pos > 0 && out[pos - 1] != '/' && !append_text(out, cap, &pos, "/"))
      return 0;
    if (!append_text(out, cap, &pos, leaf))
      return 0;
  }

  return 1;
}

int config_nio_mount_mappings(config_nio_state_t *state)
{
  uint8_t unit;
  uint8_t mounted;

  if (!state)
    return 0;

  mounted = 0;
  for (unit = 0; unit < FNCTL_MAX_UNITS; unit++) {
    config_nio_mapping_t mapping;
    config_nio_slot_t slot;

    if (!config_nio_mapping_get(state, unit, &mapping))
      continue;
    if (!mapping.valid)
      continue;
    if (!config_nio_read_slot(mapping.slot, &slot) ||
        !slot.enabled || !slot.uri[0]) {
      config_nio_set_status(state, "Mapped slot is empty");
      continue;
    }
    if (!fnsvc_disk_mount(unit, slot.uri,
                          (uint8_t) (mapping.readonly ||
                                     strcmp(slot.mode, "r") == 0),
                          CONFIG_NIO_SECTOR_SIZE_HINT)) {
      config_nio_set_status(state, mount_failure());
      continue;
    }
    if (!fnctl_set_unit_slot(unit, unit)) {
      config_nio_set_status(state, "Drive map failed");
      continue;
    }
    mounted++;
  }

  config_nio_set_status(state, mounted ? "Mounted mappings" : "No mappings mounted");
  return mounted > 0;
}
