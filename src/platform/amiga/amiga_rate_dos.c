/*
 * Amiga glue for the serial rate change (amiga_rate.c): fujinet-nio.device
 * GET_BAUD/SET_BAUD on Config's own device handle (fujinet-nio-lib keeps its
 * handle private), the boot-line check and S:FujiNet-Config save, and the
 * FujiNet probes.  Kickstart 1.3 safe: V33 DOS and Exec calls only.
 */
#include "amiga_rate.h"
#include "config_nio.h"
#include "fujinet-nio.h"
#include "fujinet_boot_config.h"
#include "fujinet_nio_device.h"
#include "fujinet_nio_endian.h"

#include <clib/alib_protos.h>
#include <dos/dos.h>
#include <exec/io.h>
#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include <string.h>

/* ---- fujinet-nio.device ------------------------------------------------- */

/* One GET_BAUD or SET_BAUD on a handle opened for just this request, as
 * fujinet-nio-baud does.  The device closes its serial backend on SET_BAUD;
 * the next exchange reopens it at the new rate. */
static uint8_t baud_request(UWORD command, uint32_t *baud)
{
  struct MsgPort *port;
  struct IORequest open_req;
  struct FujiNetNIORequest req;
  UBYTE data[4];
  uint8_t rc = FN_OK;

  port = CreatePort(NULL, 0);
  if (!port)
    return FN_ERR_IO;
  memset(&open_req, 0, sizeof(open_req));
  if (OpenDevice((CONST_STRPTR) FUJINET_NIO_DEVICE_NAME,
                 FUJINET_NIO_DEVICE_UNIT, &open_req, 0) != 0) {
    DeletePort(port);
    return FN_ERR_IO;
  }
  memset(&req, 0, sizeof(req));
  req.fn_io.io_Message.mn_Node.ln_Type = NT_MESSAGE;
  req.fn_io.io_Message.mn_ReplyPort = port;
  req.fn_io.io_Message.mn_Length = sizeof(req);
  req.fn_io.io_Command = command;
  req.fn_io.io_Device = open_req.io_Device;
  req.fn_io.io_Unit = open_req.io_Unit;
  req.fn_struct_size = FUJINET_NIO_REQUEST_SIZE;
  if (command == FUJINET_NIO_CMD_SET_BAUD) {
    fujinet_nio_put_le32(data, *baud);
    req.fn_request_data = data;
    req.fn_request_length = sizeof(data);
  } else {
    req.fn_response_data = data;
    req.fn_response_capacity = sizeof(data);
  }
  if (DoIO(&req.fn_io) != 0 || req.fn_nio_error != 0)
    rc = req.fn_nio_error ? req.fn_nio_error : FN_ERR_IO;
  else if (command == FUJINET_NIO_CMD_GET_BAUD) {
    if (req.fn_response_length != sizeof(data))
      rc = FN_ERR_IO;
    else
      *baud = fujinet_nio_get_le32(data);
  }
  CloseDevice(&open_req);
  DeletePort(port);
  return rc;
}

uint8_t amiga_rate_get(uint32_t *baud)
{
  return baud_request(FUJINET_NIO_CMD_GET_BAUD, baud);
}

uint8_t amiga_rate_set(uint32_t baud)
{
  return baud_request(FUJINET_NIO_CMD_SET_BAUD, &baud);
}

/* ---- Startup files (read only) and S:FujiNet-Config --------------------- */

/* Whether one startup file runs the boot line.  Read in line-aligned chunks,
 * so a boot line anywhere in a long S:Startup-Sequence is found; a line
 * longer than the buffer is skipped (it cannot be the boot line). */
static int file_uses_boot_line(const char *path)
{
  static char buf[2 * FUJINET_BOOT_CONFIG_MAX];
  BPTR fh = Open((CONST_STRPTR) path, MODE_OLDFILE);
  uint16_t have = 0;
  int skipping = 0;
  int found = 0;

  if (!fh)
    return 0;
  for (;;) {
    LONG got = Read(fh, buf + have, (LONG) (sizeof(buf) - have));
    uint16_t end;

    if (got < 0)
      break;
    have = (uint16_t) (have + got);
    if (skipping) {
      uint16_t i = 0;

      while (i < have && buf[i] != '\n')
        i++;
      if (i == have) {
        have = 0;
        if (got == 0)
          break;
        continue;
      }
      i++;
      memmove(buf, buf + i, have - i);
      have = (uint16_t) (have - i);
      skipping = 0;
    }
    if (got == 0) {
      found = have && fujinet_boot_config_startup_uses(buf, have);
      break;
    }
    end = have;
    while (end > 0 && buf[end - 1] != '\n')
      end--;
    if (end == 0) {
      if (have == sizeof(buf)) {
        skipping = 1;
        have = 0;
      }
      continue;
    }
    if (fujinet_boot_config_startup_uses(buf, end)) {
      found = 1;
      break;
    }
    memmove(buf, buf + end, have - end);
    have = (uint16_t) (have - end);
  }
  Close(fh);
  return found;
}

int amiga_rate_boot_line_present(void)
{
  static const char *const files[] = {
    "S:Startup-Sequence", "S:User-Startup", "S:StartupII"
  };
  unsigned i;

  for (i = 0; i < sizeof(files) / sizeof(files[0]); i++)
    if (file_uses_boot_line(files[i]))
      return 1;
  return 0;
}

#define RATE_CONFIG_NEW FUJINET_BOOT_CONFIG_PATH ".new"

/* Before switching: whether the save can create S:FujiNet-Config.new.
 * A write-protected disk, or one too full for a new file, fails here. */
int amiga_rate_can_save(void)
{
  BPTR fh = Open((CONST_STRPTR) RATE_CONFIG_NEW, MODE_NEWFILE);

  if (!fh)
    return 0;
  Close(fh);
  (void) DeleteFile((CONST_STRPTR) RATE_CONFIG_NEW);
  return 1;
}

/* Rewrites S:FujiNet-Config with BAUD=<baud>, keeping its other lines:
 * write S:FujiNet-Config.new, delete the old file, rename (Rename does not
 * overwrite on 1.3).  A missing file starts empty; one larger than the boot
 * tools read is left alone (its tail would be lost). */
int amiga_rate_save(uint32_t baud)
{
  static char in[FUJINET_BOOT_CONFIG_MAX];
  static char out[FUJINET_BOOT_CONFIG_MAX];
  LONG got = 0;
  uint16_t n;
  BPTR fh;

  fh = Open((CONST_STRPTR) FUJINET_BOOT_CONFIG_PATH, MODE_OLDFILE);
  if (fh) {
    char extra;

    got = Read(fh, in, sizeof(in));
    if (got == (LONG) sizeof(in) && Read(fh, &extra, 1) != 0)
      got = -1;
    Close(fh);
    if (got < 0)
      return 0;
  } else if (IoErr() != ERROR_OBJECT_NOT_FOUND) {
    return 0;
  }
  n = fujinet_boot_config_set_baud(in, (uint16_t) got, baud, out, sizeof(out));
  if (n == 0)
    return 0;

  fh = Open((CONST_STRPTR) RATE_CONFIG_NEW, MODE_NEWFILE);
  if (!fh)
    return 0;
  got = Write(fh, out, n);
  Close(fh);
  if (got != (LONG) n) {
    (void) DeleteFile((CONST_STRPTR) RATE_CONFIG_NEW);
    return 0;
  }
  if (!DeleteFile((CONST_STRPTR) FUJINET_BOOT_CONFIG_PATH) &&
      IoErr() != ERROR_OBJECT_NOT_FOUND) {
    (void) DeleteFile((CONST_STRPTR) RATE_CONFIG_NEW);
    return 0;
  }
  return Rename((CONST_STRPTR) RATE_CONFIG_NEW,
                (CONST_STRPTR) FUJINET_BOOT_CONFIG_PATH) != 0;
}

/* ---- FujiNet probes ------------------------------------------------------ */

static ULONG ticks_now(void)
{
  struct DateStamp ds;

  DateStamp(&ds);
  return ((ULONG) ds.ds_Days * 1440UL + (ULONG) ds.ds_Minute) *
         (60UL * TICKS_PER_SECOND) + (ULONG) ds.ds_Tick;
}

/* The request Refresh uses for the Firmware row.  A FujiNet whose firmware
 * predates it still answers (FN_ERR_UNSUPPORTED), which proves the link. */
uint8_t amiga_rate_probe_small(void)
{
  fn_fuji_info_t info;
  ULONG start = ticks_now();
  uint8_t rc = fn_fuji_get_info(&info);

  if (rc == FN_ERR_UNSUPPORTED)
    return FN_OK;
  /* An error returned at once (no timeout) must not spin on the port. */
  if (rc != FN_OK && rc != FN_ERR_ABORTED &&
      ticks_now() - start < TICKS_PER_SECOND)
    Delay(TICKS_PER_SECOND / 2);
  return rc;
}

#define RATE_CHECK_KEY "rate-check"
#define RATE_CHECK_LEN 512U

static uint8_t check_io_buf[1024];
static fn_appstore_io_t check_io = { check_io_buf, sizeof(check_io_buf) };
static uint8_t check_data[RATE_CHECK_LEN];
static uint8_t check_written;   /* the pattern is on the FujiNet */
static uint8_t check_tried;     /* a write was sent: the key may exist */

void amiga_rate_probe_reset(void)
{
  check_written = 0;
  check_tried = 0;
}

/* One 512-byte round trip through the app store (config-nio/rate-check):
 * the first call in a change writes the pattern, every call reads it back
 * and compares.  A failed write (no app store) fails the check. */
uint8_t amiga_rate_probe_large(void)
{
  fn_appstore_read_t rr;
  uint16_t i;
  uint8_t rc;

  if (!check_written) {
    fn_appstore_write_t wr;

    for (i = 0; i < RATE_CHECK_LEN; i++)
      check_data[i] = (uint8_t) (i ^ 0x5a);
    check_tried = 1;
    rc = fn_appstore_write(&check_io, CONFIG_NIO_NS, RATE_CHECK_KEY, 0,
                           check_data, RATE_CHECK_LEN, &wr);
    if (rc != FN_OK)
      return rc;
    if (wr.bytes_written != RATE_CHECK_LEN)
      return FN_ERR_IO;
    check_written = 1;
  }
  memset(check_data, 0, sizeof(check_data));
  rc = fn_appstore_read(&check_io, CONFIG_NIO_NS, RATE_CHECK_KEY, 0,
                        check_data, RATE_CHECK_LEN, &rr);
  if (rc != FN_OK)
    return rc;
  if ((rr.flags & FN_APPSTORE_READ_EXISTS) == 0 ||
      rr.bytes_read != RATE_CHECK_LEN)
    return FN_ERR_IO;
  for (i = 0; i < RATE_CHECK_LEN; i++)
    if (check_data[i] != (uint8_t) (i ^ 0x5a))
      return FN_ERR_IO;
  return FN_OK;
}

/* After the flow, with the wait hook off: remove the check key. */
void amiga_rate_probe_cleanup(void)
{
  fn_appstore_delete_t del;

  if (check_tried)
    (void) fn_appstore_delete(&check_io, CONFIG_NIO_NS, RATE_CHECK_KEY, &del);
  amiga_rate_probe_reset();
}
