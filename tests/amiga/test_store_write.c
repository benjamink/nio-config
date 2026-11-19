#include "check.h"
#include "fake_nio.h"
#include "config_nio.h"

static config_nio_state_t state;

/* Saving a shorter value must not keep the old tail: offset writes never truncate, so the store deletes the key
 * first (docs/app_store_service_protocol.md). */
void test_store_write(void)
{
  static const char old_hosts[] = "sd0:/\nfujinet.diller.org\nfujinet.online\ntnfs://example.org\n";
  const uint8_t *v;
  uint16_t len;

  fake_nio_reset();
  CHECK(config_nio_load(&state));
  fake_appstore_put(CONFIG_NIO_NS, CONFIG_NIO_KEY_HOSTS, old_hosts, (uint16_t) (sizeof(old_hosts) - 1));

  state.host_count = 0;
  CHECK(config_nio_host_set(&state, 0, "a"));
  state.host_count = 1;
  CHECK(config_nio_save_hosts(&state));

  v = fake_appstore_get(CONFIG_NIO_NS, CONFIG_NIO_KEY_HOSTS, &len);
  CHECK(v != NULL);
  CHECK(len == 2);
  CHECK(v && memcmp(v, "a\n", 2) == 0);
}
