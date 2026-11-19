/* Exercise the real ListPages/ListDirectory parser, independently of the
 * controller fake. The replies are literal FileService wire records. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fnctl.h"
#include "fnsvc.h"

static uint8_t reply[1024];
static uint16_t reply_len;
static uint8_t command_expected;
static uint8_t received_flags;
static char received_name[221];
static unsigned calls;
static uint8_t last_request[64];

uint16_t fnctl_last_dos_error(void) { return 0; }
int fnctl_nio_call(uint8_t device, uint8_t command, const void *request,
                   uint16_t request_len, void *response, uint16_t capacity,
                   uint8_t *status, uint16_t *response_len)
{
  memcpy(last_request, request, request_len < sizeof(last_request) ? request_len : sizeof(last_request));
  assert(device == 0xfe && command == command_expected && capacity >= reply_len);
  memcpy(response, reply, reply_len);
  *response_len = reply_len;
  *status = 0;
  return 1;
}

static void record(uint8_t flags, const char *name, uint32_t size,
                   uint32_t mtime, void *ctx)
{
  (void) size; (void) mtime; (void) ctx;
  ++calls;
  received_flags = flags;
  strcpy(received_name, name);
}

static void page_reply(uint8_t entry_flags, uint8_t name_len)
{
  uint16_t data_len = (uint16_t) (2 + name_len + 8);
  memset(reply, 0, sizeof(reply));
  reply[0] = 1; reply[4] = 1; reply[10] = 4; reply[11] = 1;
  reply[12] = 1; reply[13] = (uint8_t) data_len; reply[14] = (uint8_t) (data_len >> 8);
  reply[15] = entry_flags; reply[16] = name_len;
  memset(reply + 17, 'x', name_len);
  reply_len = (uint16_t) (15 + data_len);
  command_expected = 7;
  calls = 0;
}

static void directory_reply(uint8_t entry_flags)
{
  memset(reply, 0, sizeof(reply));
  reply[0] = 1; reply[6] = 1; reply[8] = 19;
  reply[10] = entry_flags; reply[11] = 1; reply[12] = 'x';
  reply_len = 29;
  command_expected = 2;
  calls = 0;
}

int main(void)
{
  uint32_t total;
  uint16_t delivered;

  page_reply(0x80, 220);
  assert(fnsvc_list_pages("sd0:/", 0, 0, record, NULL, &total, &delivered));
  assert(calls == 1 && total == 1 && delivered == 1);
  assert(received_flags == 0x80 && strlen(received_name) == 220);
  page_reply(0x81, 1);
  assert(fnsvc_list_pages("sd0:/", 0, 0, record, NULL, &total, &delivered));
  assert(received_flags == 0x81);

  /* The ListPages page index is first_entry / 4 across the 0x10000 boundaries (computed without a 32-bit
   * shift for z88dk). It follows the URI: version(1) + uri_len(2) + "sd0:/"(5). */
  {
    static const uint32_t entries[] = { 0, 4, 0xFFFC, 0x10000, 0x10004, 0x1FFFC, 0x20000, 0x2FFFC, 0x30000,
                                        0x3FFFC };
    unsigned i;

    for (i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
      page_reply(0x80, 1);
      assert(fnsvc_list_pages("sd0:/", entries[i], 0, record, NULL, &total, &delivered));
      assert((uint16_t) (last_request[8] | (last_request[9] << 8)) == entries[i] / 4);
    }
    assert(!fnsvc_list_pages("sd0:/", 0x40000UL, 0, record, NULL, &total, &delivered));
  }

  directory_reply(0x80);
  assert(fnsvc_list_directory_flags("sd0:/", record, NULL));
  assert(calls == 1 && received_flags == 0x80);
  directory_reply(0x80);
  assert(fnsvc_list_directory("sd0:/", record, NULL));
  assert(calls == 1 && received_flags == 0); /* legacy boolean contract */
  puts("Amiga FileService wire tests passed");
  return 0;
}
