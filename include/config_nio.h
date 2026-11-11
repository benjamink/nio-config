#ifndef CONFIG_NIO_H
#define CONFIG_NIO_H

#include "fnctl.h"
#include "fnsvc.h"

#include <stdint.h>

#define CONFIG_NIO_NS "config-nio"
#define CONFIG_NIO_KEY_HOSTS "hosts"
#define CONFIG_NIO_KEY_MAPPINGS "mappings"
#define CONFIG_NIO_KEY_PREFS "prefs"
#define CONFIG_NIO_SLOT_KEY_PREFIX "slot-"

#define CONFIG_NIO_PREF_DATE_YMD 0
#define CONFIG_NIO_PREF_DATE_YDM 1
#define CONFIG_NIO_PREF_SIZE_FULL 0
#define CONFIG_NIO_PREF_SIZE_COMPACT 1
#define CONFIG_NIO_ENTRY_FLAG_DIR 0x01
#define CONFIG_NIO_ENTRY_FLAG_NAME_TRUNCATED 0x80

#define CONFIG_NIO_COLOR_BODY 0
#define CONFIG_NIO_COLOR_FRAME 1
#define CONFIG_NIO_COLOR_TITLE 2
#define CONFIG_NIO_COLOR_SELECT 3
#define CONFIG_NIO_COLOR_STATUS 4
#define CONFIG_NIO_COLOR_INACTIVE 5
#define CONFIG_NIO_COLOR_INACTIVE_SELECT 6
#define CONFIG_NIO_COLOR_MENUBAR 7
#define CONFIG_NIO_COLOR_MENUHOT 8
#define CONFIG_NIO_COLOR_TITLEBAR 9
#define CONFIG_NIO_COLOR_BUTTON 10
#define CONFIG_NIO_COLOR_BUTTON_SELECT 11

#ifdef CONFIG_NIO_EXTERNAL_TABLE_STATE
#include "config_nio_state_external.h"
#else
#include "config_nio_state_embedded.h"
#endif

int config_nio_load(config_nio_state_t *state);

/* After config_nio_load fails: the step that failed ("host list", ...) and
 * the FujiNet error code behind it (0 when it was not a device error). */
const char *config_nio_load_error(uint8_t *code);
int config_nio_save_hosts(const config_nio_state_t *state);
int config_nio_save_mappings(const config_nio_state_t *state);
int config_nio_save_prefs(const config_nio_state_t *state);
int config_nio_refresh_slots(config_nio_state_t *state);
int config_nio_read_slot(uint8_t index, config_nio_slot_t *slot);
int config_nio_write_slot(config_nio_state_t *state, uint8_t index,
                          const char *uri, const char *mode);
int config_nio_delete_slot(config_nio_state_t *state, uint8_t index);
int config_nio_browse(config_nio_state_t *state, uint8_t host);
int config_nio_compose_uri(const char *host, const char *path,
                           const char *leaf, char *out, uint16_t cap);
int config_nio_mount_mappings(config_nio_state_t *state);
void config_nio_run(config_nio_state_t *state);
void config_nio_fatal_message(const char *message);
int config_nio_ui_run(config_nio_state_t *state);
#ifdef CONFIG_NIO_EXTERNAL_TABLE_STATE
int fnsvc_config_nio_list_directory_page(config_nio_state_t *state,
                                         const char *uri,
                                         uint16_t start,
                                         uint8_t max_entries,
                                         uint16_t *next_start,
                                         uint8_t *more);
#endif

void config_nio_ui_clear(void);
void config_nio_ui_header(const char *title, const char *hint);
void config_nio_ui_status(const char *status);
void config_nio_ui_pause(void);
int config_nio_ui_get_key(void);
int config_nio_ui_prompt(const char *label, char *buf, uint16_t cap);
void config_nio_ui_putc(char c);
void config_nio_ui_print(const char *s);
void config_nio_ui_println(const char *s);
void config_nio_ui_print_uint(unsigned value);
void config_nio_ui_print_ulong(unsigned long value);
void config_nio_ui_print_padded(const char *s, uint8_t width);
const char *config_nio_ui_platform_name(void);
uint8_t config_nio_ui_screen_width(void);
uint8_t config_nio_ui_screen_height(void);
void config_nio_ui_drive_label(uint8_t unit, char *buf, uint8_t cap);
int config_nio_ui_show_mappings(config_nio_state_t *state);

#endif
