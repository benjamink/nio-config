/* fn_raw_call (fujinet-nio-lib's raw request) for the Model 4/4P, over the nioclient code.
 * It replaces the library's fn_raw.c, which would keep a second pair of packet buffers and
 * do the SLIP exchange in C; nioclient does both in assembly. */
#include "fn_raw.h"
#include "fujinet-nio.h"
#include "fnctl.h"

uint8_t fn_raw_call(uint8_t device, uint8_t command,
                    const void *payload, uint16_t payload_length,
                    void *reply, uint16_t reply_capacity,
                    fn_raw_response_t *response)
{
  uint8_t status;
  uint16_t len;

  if (response) {
    response->status = FN_ERR_INTERNAL;
    response->payload_length = 0;
  }
  if (payload_length > 0 && payload == 0)
    return FN_ERR_INVALID;
  if (!fnctl_nio_call(device, command, payload, payload_length, reply, reply_capacity, &status, &len))
    return fnctl_last_dos_error() == 1 ? FN_ERR_TIMEOUT : FN_ERR_TRANSPORT;
  if (response) {
    response->status = status;
    response->payload_length = len;
  }
  return FN_OK;
}
