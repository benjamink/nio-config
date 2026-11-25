#include "amiga_rate.h"
#include "amiga_fmt.h"
#include "fujinet-nio.h"

const uint32_t amiga_rate_choices[AMIGA_RATE_COUNT] = {
  9600UL, 19200UL, 38400UL, 57600UL, 115200UL
};

int amiga_rate_should_abort(const amiga_rate_ops_t *ops, void *ctx, int mode)
{
  if (mode == AMIGA_RATE_ABORT_OFF)
    return 0;
  if (ops->cancelled(ctx))
    return 1;
  return mode == AMIGA_RATE_ABORT_ANY &&
         ops->elapsed_secs(ctx) >= AMIGA_RATE_WAIT_SECS;
}

static void set_abortable(const amiga_rate_ops_t *ops, void *ctx, int mode)
{
  if (ops->abortable) ops->abortable(mode, ctx);
}

/* Back to the old rate before the FujiNet ever answered at the new one: it
 * is presumably still at the old rate, so one small probe confirms it. */
static amiga_rate_result_t revert(const amiga_rate_ops_t *ops, void *ctx,
                                  uint32_t old_baud, amiga_rate_result_t why)
{
  set_abortable(ops, ctx, AMIGA_RATE_ABORT_OFF);
  ops->set_baud(old_baud, ctx);
  if (ops->probe_small(ctx) != FN_OK) return AMIGA_RATE_LOST;
  return why;
}

/* Back to the old rate after the FujiNet answered at the new one: the user
 * has changed it (and probably saved it), so unless the old rate answers at
 * once, wait again, up to 60 s, for the user to set it back. */
static amiga_rate_result_t revert_answered(const amiga_rate_ops_t *ops,
                                           void *ctx, uint32_t old_baud,
                                           amiga_rate_result_t why)
{
  amiga_rate_result_t r = AMIGA_RATE_LOST_ANSWERED;

  set_abortable(ops, ctx, AMIGA_RATE_ABORT_OFF);
  ops->set_baud(old_baud, ctx);
  if (ops->probe_small(ctx) == FN_OK) return why;

  ops->wait_back(old_baud, ctx);
  set_abortable(ops, ctx, AMIGA_RATE_ABORT_CANCEL);
  while (!ops->cancelled(ctx) &&
         ops->elapsed_secs(ctx) < AMIGA_RATE_WAIT_SECS) {
    uint8_t rc = ops->probe_small(ctx);
    if (rc == FN_OK) {
      r = why;
      break;
    }
    if (rc == FN_ERR_ABORTED) break;
  }
  set_abortable(ops, ctx, AMIGA_RATE_ABORT_OFF);
  return r;
}

amiga_rate_result_t amiga_rate_change(const amiga_rate_ops_t *ops, void *ctx,
                                      uint32_t old_baud, uint32_t new_baud,
                                      int persist)
{
  unsigned i;

  if (new_baud == old_baud) return AMIGA_RATE_UNCHANGED;
  if (ops->set_baud(new_baud, ctx) != FN_OK) return AMIGA_RATE_SET_FAILED;
  set_abortable(ops, ctx, AMIGA_RATE_ABORT_ANY);

  for (;;) {
    uint8_t rc;
    if (ops->cancelled(ctx))
      return revert(ops, ctx, old_baud, AMIGA_RATE_CANCELLED);
    if (ops->elapsed_secs(ctx) >= AMIGA_RATE_WAIT_SECS)
      return revert(ops, ctx, old_baud, AMIGA_RATE_TIMED_OUT);
    rc = ops->probe_small(ctx);
    if (rc == FN_OK) break;
    if (rc == FN_ERR_ABORTED) {
      /* Aborted by the hook: cancel, else the deadline passed inside it. */
      return revert(ops, ctx, old_baud,
                    ops->cancelled(ctx) ? AMIGA_RATE_CANCELLED
                                        : AMIGA_RATE_TIMED_OUT);
    }
  }

  /* The FujiNet answered at the new rate.  The large check gets all its
   * tries whatever the clock says; only Cancel cuts it short. */
  set_abortable(ops, ctx, AMIGA_RATE_ABORT_CANCEL);
  for (i = 0; i < AMIGA_RATE_LARGE_TRIES; i++) {
    if (ops->cancelled(ctx))
      return revert_answered(ops, ctx, old_baud, AMIGA_RATE_CANCELLED);
    if (ops->probe_large(ctx) != FN_OK)
      return revert_answered(ops, ctx, old_baud,
                             ops->cancelled(ctx) ? AMIGA_RATE_CANCELLED
                                                 : AMIGA_RATE_UNRELIABLE);
  }

  set_abortable(ops, ctx, AMIGA_RATE_ABORT_OFF);
  if (!persist) return AMIGA_RATE_SESSION;
  return ops->save(new_baud, ctx) ? AMIGA_RATE_SAVED : AMIGA_RATE_NOT_SAVED;
}

void amiga_rate_result_text(amiga_rate_result_t r, uint32_t old_baud,
                            uint32_t new_baud, char *out)
{
  unsigned long o = (unsigned long) old_baud;
  unsigned long n = (unsigned long) new_baud;

  switch (r) {
  case AMIGA_RATE_SAVED:
    amiga_sprintf(out, "Serial rate %lu saved", n); break;
  case AMIGA_RATE_SESSION:
    amiga_sprintf(out, "Serial rate %lu for this session only", n); break;
  case AMIGA_RATE_NOT_SAVED:
    amiga_sprintf(out, "Rate %lu not saved: set FujiNet to %lu before reboot", n, o); break;
  case AMIGA_RATE_TIMED_OUT:
    amiga_sprintf(out, "No answer at %lu; back to %lu", n, o); break;
  case AMIGA_RATE_CANCELLED:
    amiga_sprintf(out, "Cancelled; back to %lu", o); break;
  case AMIGA_RATE_UNRELIABLE:
    amiga_sprintf(out, "%lu lost data on long replies; back to %lu", n, o); break;
  case AMIGA_RATE_LOST:
    amiga_sprintf(out, "No answer at %lu either: set the FujiNet back to %lu", n, o); break;
  case AMIGA_RATE_LOST_ANSWERED:
    amiga_sprintf(out, "Set the FujiNet back: uart.baud %lu then uart.save", o); break;
  case AMIGA_RATE_SET_FAILED:
    amiga_sprintf(out, "Could not set the serial rate"); break;
  case AMIGA_RATE_UNCHANGED:
  default:
    amiga_sprintf(out, "Serial rate is already %lu", o); break;
  }
}

void amiga_rate_ask_text(amiga_rate_ask_t ask, uint32_t old_baud,
                         uint32_t new_baud, char *out)
{
  unsigned long o = (unsigned long) old_baud;
  unsigned long n = (unsigned long) new_baud;

  switch (ask) {
  case AMIGA_RATE_ASK_NO_BOOT_LINE:
    amiga_sprintf(out, "This Amiga's startup does not read S:FujiNet-Config, "
                  "so the new rate would be lost at the next boot.");
    break;
  case AMIGA_RATE_ASK_NO_WRITE:
    amiga_sprintf(out, "S:FujiNet-Config cannot be written (disk "
                  "write-protected or full), so the new rate would be lost "
                  "at the next boot.");
    break;
  case AMIGA_RATE_ASK_SESSION:
    amiga_sprintf(out, "Config switches the Amiga to %lu now. Then, on the "
                  "FujiNet's console, type: uart.baud %lu (not uart.save). "
                  "Config waits 60 seconds and switches back to %lu if the "
                  "FujiNet does not answer.\n"
                  "FujiNet drives may not respond while waiting.", n, n, o);
    break;
  case AMIGA_RATE_ASK_CONFIRM:
  default:
    amiga_sprintf(out, "Config switches the Amiga to %lu now. Then, on the "
                  "FujiNet's console, type: uart.baud %lu and uart.save. "
                  "Config waits 60 seconds and switches back to %lu if the "
                  "FujiNet does not answer.\n"
                  "FujiNet drives may not respond while waiting.", n, n, o);
    break;
  }
}
