#ifndef AMIGA_TEST_FAKE_NIO_H
#define AMIGA_TEST_FAKE_NIO_H

#include <stdint.h>

typedef struct {
  uint8_t is_dir;
  const char *name;
  uint32_t size;
  uint32_t mtime;
} fake_dir_entry_t;

void fake_nio_reset(void);
void fake_appstore_put(const char *ns, const char *key, const void *data,
                       uint16_t len);
const uint8_t *fake_appstore_get(const char *ns, const char *key,
                                 uint16_t *len);
void fake_slot_put(uint8_t index, const char *uri, uint8_t readonly);
const char *fake_slot_uri(uint8_t index);
uint8_t fake_slot_readonly(uint8_t index);
unsigned fake_slot_get_calls(void);
void fake_dir_put(const char *uri, const fake_dir_entry_t *entries,
                  uint8_t count);
void fake_dir_fail(const char *uri);

#endif
