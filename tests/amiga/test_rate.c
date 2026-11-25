#include "check.h"
#include "amiga_rate.h"
#include "fujinet-nio.h"

#define MAXC 32

typedef struct {
  uint8_t set_ret[4];     /* return per set_baud call; default FN_OK */
  int small_fail_n;       /* first N small probes fail (-1 = all but revert) */
  uint8_t small_code;     /* code of a failed small probe */
  int large_fail_at;      /* 1-based large probe that fails, 0 = none */
  uint8_t large_code;
  int cancel_after_fails; /* cancelled() true after N failed small probes, -1 never */
  int cancel_on_large;    /* cancelled() true once a large probe was tried */
  int revert_probe_ok;
  int save_ret;
  uint32_t clock;
  int set_n, small_n, large_n, save_n, fails;
  uint32_t set_args[MAXC];
  uint32_t save_arg;
  int abortable_now;
  int abort_log[MAXC];    /* abortable state at each small probe */
  int abort_calls;
  int use_abortable;
} fake_t;

static void fake_init(fake_t *f)
{
  memset(f, 0, sizeof *f);
  f->small_code = FN_ERR_IO;
  f->large_code = FN_ERR_IO;
  f->cancel_after_fails = -1;
  f->revert_probe_ok = 1;
  f->save_ret = 1;
  f->use_abortable = 1;
}

static uint8_t f_set(uint32_t b, void *c)
{
  fake_t *f = c;
  uint8_t r = f->set_n < 4 ? f->set_ret[f->set_n] : FN_OK;
  if (f->set_n < MAXC) f->set_args[f->set_n] = b;
  f->set_n++;
  return r;
}

static uint8_t f_small(void *c)
{
  fake_t *f = c;
  int reverted = f->set_n >= 2;
  if (f->small_n < MAXC) f->abort_log[f->small_n] = f->abortable_now;
  f->small_n++;
  if (reverted) return f->revert_probe_ok ? FN_OK : FN_ERR_IO;
  if (f->small_fail_n < 0 || f->fails < f->small_fail_n) {
    f->fails++;
    f->clock += 5;
    /* the hook only aborts once Cancel was pressed */
    if (f->small_code == FN_ERR_ABORTED && f->cancel_after_fails >= 0 &&
        f->fails < f->cancel_after_fails)
      return FN_ERR_IO;
    return f->small_code;
  }
  return FN_OK;
}

static uint8_t f_large(void *c)
{
  fake_t *f = c;
  f->large_n++;
  if (f->large_fail_at && f->large_n == f->large_fail_at) return f->large_code;
  return FN_OK;
}

static uint32_t f_elapsed(void *c) { return ((fake_t *) c)->clock; }

static int f_cancelled(void *c)
{
  fake_t *f = c;
  if (f->cancel_on_large && f->large_n > 0) return 1;
  return f->cancel_after_fails >= 0 && f->fails >= f->cancel_after_fails;
}

static int f_save(uint32_t b, void *c)
{
  fake_t *f = c;
  f->save_n++;
  f->save_arg = b;
  return f->save_ret;
}

static void f_abortable(int on, void *c)
{
  fake_t *f = c;
  f->abortable_now = on;
  f->abort_calls++;
}

static amiga_rate_result_t run(fake_t *f, uint32_t o, uint32_t n, int persist)
{
  amiga_rate_ops_t ops;
  ops.set_baud = f_set;
  ops.probe_small = f_small;
  ops.probe_large = f_large;
  ops.elapsed_secs = f_elapsed;
  ops.cancelled = f_cancelled;
  ops.save = f_save;
  ops.abortable = f->use_abortable ? f_abortable : NULL;
  return amiga_rate_change(&ops, f, o, n, persist);
}

static void rate_unchanged(void)
{
  fake_t f; fake_init(&f);
  CHECK(run(&f, 38400, 38400, 1) == AMIGA_RATE_UNCHANGED);
  CHECK(f.set_n == 0 && f.small_n == 0);
}

static void rate_saved(void)
{
  fake_t f; fake_init(&f);
  CHECK(run(&f, 38400, 57600, 1) == AMIGA_RATE_SAVED);
  CHECK(f.set_n == 1 && f.set_args[0] == 57600);
  CHECK(f.large_n == 3);
  CHECK(f.save_n == 1 && f.save_arg == 57600);
}

static void rate_session(void)
{
  fake_t f; fake_init(&f);
  CHECK(run(&f, 38400, 57600, 0) == AMIGA_RATE_SESSION);
  CHECK(f.set_n == 1 && f.save_n == 0);
}

static void rate_not_saved(void)
{
  fake_t f; fake_init(&f);
  f.save_ret = 0;
  CHECK(run(&f, 38400, 57600, 1) == AMIGA_RATE_NOT_SAVED);
  CHECK(f.set_n == 1 && f.set_args[0] == 57600);
  CHECK(f.save_n == 1);
}

static void rate_timeout(void)
{
  fake_t f; fake_init(&f);
  f.small_fail_n = -1;
  CHECK(run(&f, 38400, 57600, 1) == AMIGA_RATE_TIMED_OUT);
  CHECK(f.set_n == 2 && f.set_args[0] == 57600 && f.set_args[1] == 38400);
  CHECK(f.save_n == 0 && f.large_n == 0);
}

static void rate_timeout_aborted(void)
{
  fake_t f; fake_init(&f);
  f.small_fail_n = 1;
  f.small_code = FN_ERR_ABORTED;  /* deadline passed inside the hook */
  CHECK(run(&f, 38400, 57600, 1) == AMIGA_RATE_TIMED_OUT);
  CHECK(f.set_n == 2);
}

static void rate_cancel_small(void)
{
  fake_t f; fake_init(&f);
  f.small_fail_n = -1;
  f.small_code = FN_ERR_ABORTED;
  f.cancel_after_fails = 2;
  CHECK(run(&f, 38400, 57600, 1) == AMIGA_RATE_CANCELLED);
  CHECK(f.set_n == 2 && f.set_args[1] == 38400);
  CHECK(f.save_n == 0);
}

static void rate_unreliable(void)
{
  fake_t f; fake_init(&f);
  f.large_fail_at = 2;
  CHECK(run(&f, 38400, 115200, 1) == AMIGA_RATE_UNRELIABLE);
  CHECK(f.set_n == 2 && f.set_args[1] == 38400);
  CHECK(f.save_n == 0 && f.large_n == 2);
}

static void rate_unreliable_aborted_no_cancel(void)
{
  fake_t f; fake_init(&f);
  f.large_fail_at = 1;
  f.large_code = FN_ERR_ABORTED;
  CHECK(run(&f, 38400, 115200, 1) == AMIGA_RATE_UNRELIABLE);
}

static void rate_cancel_large(void)
{
  fake_t f; fake_init(&f);
  f.large_fail_at = 1;
  f.large_code = FN_ERR_ABORTED;
  f.cancel_on_large = 1;
  CHECK(run(&f, 38400, 57600, 1) == AMIGA_RATE_CANCELLED);
  CHECK(f.set_n == 2 && f.set_args[1] == 38400);
  CHECK(f.save_n == 0);
}

static void rate_lost(void)
{
  fake_t f; fake_init(&f);
  f.small_fail_n = -1;
  f.revert_probe_ok = 0;
  CHECK(run(&f, 38400, 57600, 1) == AMIGA_RATE_LOST);
  CHECK(f.set_n == 2);
}

static void rate_set_failed(void)
{
  fake_t f; fake_init(&f);
  f.set_ret[0] = FN_ERR_IO;
  CHECK(run(&f, 38400, 57600, 1) == AMIGA_RATE_SET_FAILED);
  CHECK(f.set_n == 1 && f.small_n == 0 && f.abort_calls == 0);
}

static void rate_revert_not_abortable(void)
{
  fake_t f; fake_init(&f);
  int i;
  /* timeout */
  f.small_fail_n = -1;
  CHECK(run(&f, 38400, 57600, 1) == AMIGA_RATE_TIMED_OUT);
  CHECK(f.small_n >= 2);
  for (i = 0; i < f.small_n - 1; i++) CHECK(f.abort_log[i] == 1);
  CHECK(f.abort_log[f.small_n - 1] == 0);

  /* cancel */
  fake_init(&f);
  f.small_fail_n = -1;
  f.small_code = FN_ERR_ABORTED;
  f.cancel_after_fails = 2;
  CHECK(run(&f, 38400, 57600, 1) == AMIGA_RATE_CANCELLED);
  CHECK(f.small_n >= 2);
  for (i = 0; i < f.small_n - 1; i++) CHECK(f.abort_log[i] == 1);
  CHECK(f.abort_log[f.small_n - 1] == 0);
}

static void rate_abortable_success_and_null(void)
{
  fake_t f; fake_init(&f);
  CHECK(run(&f, 38400, 57600, 1) == AMIGA_RATE_SAVED);
  CHECK(f.abortable_now == 0 && f.abort_calls == 2);
  CHECK(f.abort_log[0] == 1);

  fake_init(&f);
  f.use_abortable = 0;
  f.small_fail_n = -1;
  CHECK(run(&f, 38400, 57600, 1) == AMIGA_RATE_TIMED_OUT);
  fake_init(&f);
  f.use_abortable = 0;
  CHECK(run(&f, 38400, 57600, 0) == AMIGA_RATE_SESSION);
}

static void rate_should_abort(void)
{
  fake_t f; fake_init(&f);
  amiga_rate_ops_t ops;
  ops.elapsed_secs = f_elapsed;
  ops.cancelled = f_cancelled;
  CHECK(amiga_rate_should_abort(&ops, &f) == 0);
  f.clock = 59;
  CHECK(amiga_rate_should_abort(&ops, &f) == 0);
  f.clock = 60;
  CHECK(amiga_rate_should_abort(&ops, &f) == 1);
  f.clock = 0;
  f.cancel_after_fails = 0;
  CHECK(amiga_rate_should_abort(&ops, &f) == 1);
}

static void rate_text(void)
{
  char out[80];
  amiga_rate_result_text(AMIGA_RATE_TIMED_OUT, 38400, 57600, out);
  CHECK_STR(out, "No answer at 57600; back to 38400");
  amiga_rate_result_text(AMIGA_RATE_SAVED, 38400, 57600, out);
  CHECK_STR(out, "Serial rate 57600 saved");
  amiga_rate_result_text(AMIGA_RATE_SESSION, 38400, 57600, out);
  CHECK_STR(out, "Serial rate 57600 for this session only");
  amiga_rate_result_text(AMIGA_RATE_NOT_SAVED, 38400, 57600, out);
  CHECK_STR(out, "Serial rate 57600 in use but not saved (disk full or write-protected?)");
  amiga_rate_result_text(AMIGA_RATE_CANCELLED, 38400, 57600, out);
  CHECK_STR(out, "Cancelled; back to 38400");
  amiga_rate_result_text(AMIGA_RATE_UNRELIABLE, 38400, 115200, out);
  CHECK_STR(out, "115200 lost data on long replies; back to 38400");
  amiga_rate_result_text(AMIGA_RATE_LOST, 38400, 57600, out);
  CHECK_STR(out, "No answer at 57600 either: set the FujiNet back to 38400");
  amiga_rate_result_text(AMIGA_RATE_SET_FAILED, 38400, 57600, out);
  CHECK_STR(out, "Could not set the serial rate");
  amiga_rate_result_text(AMIGA_RATE_UNCHANGED, 38400, 38400, out);
  CHECK_STR(out, "Serial rate is already 38400");
  CHECK(amiga_rate_choices[0] == 9600 && amiga_rate_choices[4] == 115200);
}

void test_rate(void)
{
  rate_unchanged();
  rate_saved();
  rate_session();
  rate_not_saved();
  rate_timeout();
  rate_timeout_aborted();
  rate_cancel_small();
  rate_unreliable();
  rate_unreliable_aborted_no_cancel();
  rate_cancel_large();
  rate_lost();
  rate_set_failed();
  rate_revert_not_abortable();
  rate_abortable_success_and_null();
  rate_should_abort();
  rate_text();
}
