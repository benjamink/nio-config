#ifndef CONFIG_NIO_STATE_EMBEDDED_H
#define CONFIG_NIO_STATE_EMBEDDED_H

#if defined(__CC65__)
#define CONFIG_NIO_COLOR_COUNT 1
#define CONFIG_NIO_MAX_HOSTS 4
#define CONFIG_NIO_MAX_ENTRIES 5
#define CONFIG_NIO_URI_MAX 48
#define CONFIG_NIO_NAME_MAX 31
#define CONFIG_NIO_TEXT_MAX 512
#define CONFIG_NIO_PATH_MAX FNSVC_MAX_PATH
#define CONFIG_NIO_STATUS_MAX 95
#else
#define CONFIG_NIO_COLOR_COUNT 12
#define CONFIG_NIO_MAX_HOSTS 8
#ifndef CONFIG_NIO_MAX_ENTRIES
#define CONFIG_NIO_MAX_ENTRIES 20
#endif
#define CONFIG_NIO_URI_MAX FNSVC_MAX_URI
#define CONFIG_NIO_NAME_MAX 79
#define CONFIG_NIO_TEXT_MAX 4096
#define CONFIG_NIO_PATH_MAX FNSVC_MAX_PATH
#define CONFIG_NIO_STATUS_MAX 95
#endif

typedef struct {
  uint8_t valid;
  uint8_t slot;
  uint8_t readonly;
} config_nio_mapping_t;

typedef struct {
  uint8_t enabled;
  char uri[CONFIG_NIO_URI_MAX + 1];
  char mode[4];
} config_nio_slot_t;

typedef struct {
  uint8_t is_dir;
  char name[CONFIG_NIO_NAME_MAX + 1];
  uint32_t size;
  uint32_t mtime;
} config_nio_entry_t;

typedef struct {
  uint8_t date_format;
  uint8_t size_format;
  uint8_t color_fg[CONFIG_NIO_COLOR_COUNT];
  uint8_t color_bg[CONFIG_NIO_COLOR_COUNT];
} config_nio_prefs_t;

typedef struct {
  uint8_t host_count;
  char hosts[CONFIG_NIO_MAX_HOSTS][CONFIG_NIO_URI_MAX + 1];
  config_nio_slot_t slots[FNCTL_MAX_UNITS];
  uint8_t slot_start;
  uint8_t slot_count;
  uint8_t slots_more;
  config_nio_mapping_t mappings[FNCTL_MAX_UNITS];
  config_nio_prefs_t prefs;
  config_nio_entry_t entries[CONFIG_NIO_MAX_ENTRIES];
  uint8_t entry_count;
  uint16_t entry_total;
  uint8_t entries_truncated;
  char browse_path[CONFIG_NIO_PATH_MAX + 1];
  char status[CONFIG_NIO_STATUS_MAX + 1];
} config_nio_state_t;

int config_nio_set_status(config_nio_state_t *state, const char *msg);

int config_nio_host_get(const config_nio_state_t *state, uint8_t index,
                        char *buf, uint16_t cap);
int config_nio_host_set(config_nio_state_t *state, uint8_t index,
                        const char *value);
int config_nio_host_clear(config_nio_state_t *state, uint8_t index);
int config_nio_entry_get(const config_nio_state_t *state, uint8_t index,
                         config_nio_entry_t *entry);
int config_nio_entry_set(config_nio_state_t *state, uint8_t index,
                         const config_nio_entry_t *entry);
int config_nio_slot_get(const config_nio_state_t *state, uint8_t index,
                        config_nio_slot_t *slot);
int config_nio_slot_set(config_nio_state_t *state, uint8_t index,
                        const config_nio_slot_t *slot);
int config_nio_mapping_get(const config_nio_state_t *state, uint8_t unit,
                           config_nio_mapping_t *mapping);
int config_nio_mapping_set(config_nio_state_t *state, uint8_t unit,
                           const config_nio_mapping_t *mapping);
int config_nio_mapping_clear(config_nio_state_t *state, uint8_t unit);

#endif
