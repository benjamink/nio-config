#include "amiga_rate.h"
#include "amiga_fmt.h"
#include "fujinet-nio.h"

const uint32_t amiga_rate_choices[AMIGA_RATE_COUNT] = {
  9600UL, 19200UL, 38400UL, 57600UL, 115200UL
};

int amiga_rate_should_abort(const amiga_rate_ops_t *ops, void *ctx)
{
  return ops->cancelled(ctx) || ops->elapsed_secs(ctx) >= AMIGA_RATE_WAIT_SECS;
}

static void set_abortable(const amiga_rate_ops_t *ops, void *ctx, int on)
{
  if (ops->abortable) ops->abortable(on, ctx);
}

/* Back to the old rate; one small probe confirms the FujiNet answers. */
static amiga_rate_result_t revert(const amiga_rate_ops_t *ops, void *ctx,
                                  uint32_t old_baud, amiga_rate_result_t why)
{
  set_abortable(ops, ctx, 0);
  ops->set_baud(old_baud, ctx);
  if (ops->probe_small(ctx) != FN_OK) return AMIGA_RATE_LOST;
  return why;
}

amiga_rate_result_t amiga_rate_change(const amiga_rate_ops_t *ops, void *ctx,
                                      uint32_t old_baud, uint32_t new_baud,
                                      int persist)
{
  unsigned i;

  if (new_baud == old_baud) return AMIGA_RATE_UNCHANGED;
  if (ops->set_baud(new_baud, ctx) != FN_OK) return AMIGA_RATE_SET_FAILED;
  set_abortable(ops, ctx, 1);

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

  for (i = 0; i < AMIGA_RATE_LARGE_TRIES; i++) {
    if (ops->probe_large(ctx) != FN_OK)
      return revert(ops, ctx, old_baud,
                    ops->cancelled(ctx) ? AMIGA_RATE_CANCELLED
                                        : AMIGA_RATE_UNRELIABLE);
  }

  set_abortable(ops, ctx, 0);
  if (!persist) return AMIGA_RATE_SESSION;
  return ops->save(new_baud, ctx) ? AMIGA_RATE_SAVED : AMIGA_RATE_NOT_SAVED;
}

void amiga_rate_result_text(amiga_rate_result_t r, uint32_t old_baud,
                            uint32_t new_baud, char *out)
{
  switch (r) {
  case AMIGA_RATE_SAVED:
    amiga_sprintf(out, "Serial rate %lu saved", new_baud); break;
  case AMIGA_RATE_SESSION:
    amiga_sprintf(out, "Serial rate %lu for this session only", new_baud); break;
  case AMIGA_RATE_NOT_SAVED:
    amiga_sprintf(out, "Serial rate %lu in use but not saved (disk full or write-protected?)", new_baud); break;
  case AMIGA_RATE_TIMED_OUT:
    amiga_sprintf(out, "No answer at %lu; back to %lu", new_baud, old_baud); break;
  case AMIGA_RATE_CANCELLED:
    amiga_sprintf(out, "Cancelled; back to %lu", old_baud); break;
  case AMIGA_RATE_UNRELIABLE:
    amiga_sprintf(out, "%lu lost data on long replies; back to %lu", new_baud, old_baud); break;
  case AMIGA_RATE_LOST:
    amiga_sprintf(out, "No answer at %lu either: set the FujiNet back to %lu", new_baud, old_baud); break;
  case AMIGA_RATE_SET_FAILED:
    amiga_sprintf(out, "Could not set the serial rate"); break;
  case AMIGA_RATE_UNCHANGED:
  default:
    amiga_sprintf(out, "Serial rate is already %lu", old_baud); break;
  }
}
