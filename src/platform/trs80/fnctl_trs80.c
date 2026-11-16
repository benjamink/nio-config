/* The fnctl interface (include/common/fnctl.h) for the Model 4/4P: a call to the fujinet-nio
 * device over the serial port, through the nioclient code at 8000h. MS-DOS reaches the same
 * device through its resident driver; the portable fnsvc/store code only needs fnctl_nio_call. */
#include "fnctl.h"

#include <string.h>

#define NIO_PAYLOAD_MAX 0x2FA            /* the client's request buffer holds 0x300 bytes with its 6-byte header */

extern uint8_t nio_dev, nio_cmd, nio_err, nio_stat;
extern const void *nio_ptr, *nio_rptr;
extern uint16_t nio_len, nio_rlen;
extern void nio_request_raw(void);
extern void nio_uart_init(void);

static uint16_t last_error;

void trs80_nio_init(void)
{
  nio_uart_init();
}

int fnctl_nio_call(uint8_t device, uint8_t command,
                   const void *request, uint16_t request_len,
                   void *response, uint16_t response_capacity,
                   uint8_t *nio_status, uint16_t *response_len)
{
  last_error = 0;
  if (request_len > NIO_PAYLOAD_MAX) {
    last_error = 3;
    return 0;
  }
  nio_dev = device;
  nio_cmd = command;
  nio_ptr = request;
  nio_len = request_len;
  nio_request_raw();
  if (nio_err) {
    last_error = nio_err;
    return 0;
  }
  if (nio_rlen > response_capacity) {
    last_error = 3;
    return 0;
  }
  if (nio_rlen)
    memcpy(response, nio_rptr, nio_rlen);
  *nio_status = nio_stat;
  *response_len = nio_rlen;
  return 1;
}

uint16_t fnctl_last_dos_error(void)
{
  return last_error;
}

/* LS-DOS drive :d is DiskDevice slot d+1 (the convention of FMOUNT), so there is no map to set. */
int fnctl_set_unit_slot(uint8_t unit, uint8_t slot)
{
  (void) unit;
  (void) slot;
  return 1;
}

int fnctl_get_unit_slot(uint8_t unit, uint8_t *slot)
{
  *slot = unit;
  return 1;
}
