#include "check.h"
#include "amiga_rate.h"
#include "amiga_format.h"
#include "fujinet-nio.h"

#define MAXC 64
#define OLD 38400UL
#define NEW 57600UL

/* The fake models both ends of the link.  The Amiga's rate is the last
 * set_baud that succeeded; the FujiNet's rate changes when the user types
 * uart.baud, scripted as "before small probe N" (1-based).  A probe answers
 * only when the two rates match.  A failed small probe takes 5 s; a large
 * probe takes large_secs.  The GUI's wait hook is modelled too: an exchange
 * in flight returns FN_ERR_ABORTED when amiga_rate_should_abort() says so
 * for the flow's current abortable() mode.  wait_back restarts the clock
 * and forgets Cancel, as the GUI does. */
typedef struct {
  uint32_t amiga;          /* the Amiga's rate */
  uint32_t fn;             /* the FujiNet's rate */
  int fn_new_at;           /* small probe before which the FujiNet goes to NEW */
  int fn_old_at;           /* small probe before which it goes back to OLD */
  uint8_t set_ret[4];      /* return per set_baud call; default FN_OK */
  int large_fail_at;       /* 1-based large probe that loses data, 0 = none */
  int cancel_at_small;     /* Cancel pressed during small probe N, 0 = never */
  int cancel_at_large;     /* Cancel pressed during large probe N, 0 = never */
  int cancel_at_small2;    /* Cancel pressed again during small probe N */
  uint32_t large_secs;
  int save_ret;
  uint32_t clock;
  int cancel;
  int set_n, small_n, large_n, save_n;
  uint32_t set_args[MAXC];
  int set_mode[MAXC];      /* abortable state at each set_baud */
  int small_mode[MAXC];    /* abortable state at each small probe */
  int large_mode[MAXC];    /* abortable state at each large probe */
  uint32_t save_arg;
  int mode;                /* abortable state now */
  int abort_calls;
  int use_abortable;
  int back_n;              /* wait_back calls */
  uint32_t back_arg;
  int back_at_small;       /* small probes before wait_back */
  const amiga_rate_ops_t *ops;
} fake_t;

static void fake_init(fake_t *f)
{
  memset(f, 0, sizeof *f);
  f->amiga = OLD;
  f->fn = OLD;
  f->large_secs = 1;
  f->save_ret = 1;
  f->use_abortable = 1;
}

/* What the GUI hook returns for an exchange in flight. */
static int f_hook_aborts(fake_t *f)
{
  return amiga_rate_should_abort(f->ops, f, f->mode);
}

static uint8_t f_set(uint32_t b, void *c)
{
  fake_t *f = c;
  uint8_t r = f->set_n < 4 ? f->set_ret[f->set_n] : FN_OK;
  if (f->set_n < MAXC) {
    f->set_args[f->set_n] = b;
    f->set_mode[f->set_n] = f->mode;
  }
  f->set_n++;
  if (r == FN_OK) f->amiga = b;
  return r;
}

static uint8_t f_small(void *c)
{
  fake_t *f = c;
  f->small_n++;
  if (f->small_n == f->fn_new_at) f->fn = NEW;
  if (f->small_n == f->fn_old_at) f->fn = OLD;
  if (f->small_n < MAXC) f->small_mode[f->small_n - 1] = f->mode;
  if (f->amiga == f->fn) return FN_OK;
  f->clock += 5;
  if (f->small_n == f->cancel_at_small || f->small_n == f->cancel_at_small2)
    f->cancel = 1;
  return f_hook_aborts(f) ? FN_ERR_ABORTED : FN_ERR_IO;
}

static uint8_t f_large(void *c)
{
  fake_t *f = c;
  f->large_n++;
  if (f->large_n < MAXC) f->large_mode[f->large_n - 1] = f->mode;
  f->clock += f->large_secs;
  if (f->large_n == f->cancel_at_large) f->cancel = 1;
  if (f_hook_aborts(f)) return FN_ERR_ABORTED;
  if (f->amiga != f->fn || f->large_n == f->large_fail_at) return FN_ERR_IO;
  return FN_OK;
}

static uint32_t f_elapsed(void *c) { return ((fake_t *) c)->clock; }

static int f_cancelled(void *c) { return ((fake_t *) c)->cancel; }

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
  f->mode = on;
  f->abort_calls++;
}

static void f_wait_back(uint32_t b, void *c)
{
  fake_t *f = c;
  f->back_n++;
  f->back_arg = b;
  f->back_at_small = f->small_n;
  f->clock = 0;
  f->cancel = 0;
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
  ops.wait_back = f_wait_back;
  f->ops = &ops;
  f->amiga = o;
  f->fn = o;
  return amiga_rate_change(&ops, f, o, n, persist);
}

static void rate_unchanged(void)
{
  fake_t f; fake_init(&f);
  CHECK(run(&f, OLD, OLD, 1) == AMIGA_RATE_UNCHANGED);
  CHECK(f.set_n == 0 && f.small_n == 0);
}

static void rate_saved(void)
{
  fake_t f; fake_init(&f);
  f.fn_new_at = 3;               /* two probes go unanswered first */
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_SAVED);
  CHECK(f.set_n == 1 && f.set_args[0] == NEW);
  CHECK(f.small_n == 3 && f.large_n == 3);
  CHECK(f.save_n == 1 && f.save_arg == NEW);
}

static void rate_session(void)
{
  fake_t f; fake_init(&f);
  f.fn_new_at = 1;
  CHECK(run(&f, OLD, NEW, 0) == AMIGA_RATE_SESSION);
  CHECK(f.set_n == 1 && f.save_n == 0);
}

static void rate_not_saved(void)
{
  fake_t f; fake_init(&f);
  f.fn_new_at = 1;
  f.save_ret = 0;
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_NOT_SAVED);
  CHECK(f.set_n == 1 && f.set_args[0] == NEW);
  CHECK(f.save_n == 1);
}

static void rate_timeout(void)
{
  fake_t f; fake_init(&f);      /* the FujiNet never changes */
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_TIMED_OUT);
  CHECK(f.set_n == 2 && f.set_args[0] == NEW && f.set_args[1] == OLD);
  CHECK(f.save_n == 0 && f.large_n == 0);
  CHECK(f.amiga == OLD);
}

static void rate_timeout_aborted(void)
{
  fake_t f; fake_init(&f);
  f.clock = 57;                  /* the deadline passes inside the first probe */
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_TIMED_OUT);
  CHECK(f.set_n == 2 && f.small_n == 2);
}

static void rate_cancel_small(void)
{
  fake_t f; fake_init(&f);
  f.cancel_at_small = 2;
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_CANCELLED);
  CHECK(f.set_n == 2 && f.set_args[1] == OLD);
  CHECK(f.small_n == 3 && f.save_n == 0);
}

/* The FujiNet answered at the new rate, so the user has changed it and
 * probably saved it; going back must wait for the user to change it back. */
static void rate_unreliable_answered(void)
{
  fake_t f; fake_init(&f);
  f.fn_new_at = 2;
  f.large_fail_at = 2;
  f.fn_old_at = 5;               /* revert probe 3 fails, 4 fails, 5 answers */
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_UNRELIABLE);
  CHECK(f.set_n == 2 && f.set_args[1] == OLD);
  CHECK(f.save_n == 0 && f.large_n == 2);
  CHECK(f.small_n == 5);
}

static void rate_cancel_large_answered(void)
{
  fake_t f; fake_init(&f);
  f.fn_new_at = 1;
  f.cancel_at_large = 1;
  f.fn_old_at = 3;
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_CANCELLED);
  CHECK(f.set_n == 2 && f.set_args[1] == OLD);
  CHECK(f.save_n == 0 && f.large_n == 1 && f.small_n == 3);
}

/* Once the small probe passed, the deadline no longer cuts the large check
 * short: it gets all its tries. */
static void rate_deadline_in_large(void)
{
  fake_t f; fake_init(&f);
  f.fn_new_at = 12;              /* 11 unanswered probes: 55 s */
  f.large_secs = 3;              /* large checks at 58, 61 and 64 s */
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_SAVED);
  CHECK(f.large_n == 3 && f.set_n == 1 && f.save_n == 1);
}

/* The user typed uart.baud just too late: the FujiNet is at the new rate
 * when the Amiga goes back, and had never answered there. */
static void rate_lost(void)
{
  fake_t f; fake_init(&f);
  f.fn_new_at = 13;              /* the revert's own check */
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_LOST);
  CHECK(f.set_n == 2 && f.small_n == 13 && f.large_n == 0);
}

/* UNRELIABLE: the old rate answers at once (the FujiNet was set back before
 * the revert), so there is no second wait. */
static void rate_unreliable_no_second_wait(void)
{
  fake_t f; fake_init(&f);
  f.fn_new_at = 1;
  f.large_fail_at = 3;
  f.fn_old_at = 2;
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_UNRELIABLE);
  CHECK(f.back_n == 0 && f.small_n == 2 && f.large_n == 3);
}

/* The second wait shows the old rate, starts a fresh 60 s, and probes the
 * old rate until it answers. */
static void rate_second_wait(void)
{
  fake_t f; fake_init(&f);
  int i;
  f.fn_new_at = 12;              /* answered at 55 s */
  f.large_fail_at = 1;
  f.fn_old_at = 20;              /* revert check 13 fails; 14..19 fail; 20 */
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_UNRELIABLE);
  CHECK(f.back_n == 1 && f.back_arg == OLD && f.back_at_small == 13);
  CHECK(f.small_n == 20 && f.set_n == 2 && f.save_n == 0);
  CHECK(f.small_mode[12] == AMIGA_RATE_ABORT_OFF);  /* the revert's check */
  for (i = 13; i < 20; i++) CHECK(f.small_mode[i] == AMIGA_RATE_ABORT_CANCEL);
  CHECK(f.mode == AMIGA_RATE_ABORT_OFF);
}

/* Second wait never answered: 12 probes of 5 s, then LOST_ANSWERED. */
static void rate_lost_answered_timeout(void)
{
  fake_t f; fake_init(&f);
  f.fn_new_at = 1;
  f.large_fail_at = 2;
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_LOST_ANSWERED);
  CHECK(f.back_n == 1 && f.back_at_small == 2);
  CHECK(f.small_n == 2 + 12 && f.clock == 60);
  CHECK(f.set_n == 2 && f.amiga == OLD && f.save_n == 0);
  CHECK(f.mode == AMIGA_RATE_ABORT_OFF);
}

/* Cancel in the large check, then the FujiNet is set back: CANCELLED. */
static void rate_cancel_large_second_wait(void)
{
  fake_t f; fake_init(&f);
  f.fn_new_at = 1;
  f.cancel_at_large = 2;
  f.fn_old_at = 4;
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_CANCELLED);
  CHECK(f.large_n == 2 && f.back_n == 1 && f.small_n == 4);
  CHECK(f.large_mode[0] == AMIGA_RATE_ABORT_CANCEL);
  CHECK(f.set_mode[1] == AMIGA_RATE_ABORT_OFF);
}

/* Cancel in the large check, then Cancel again in the second wait. */
static void rate_cancel_second_wait(void)
{
  fake_t f; fake_init(&f);
  f.fn_new_at = 1;
  f.cancel_at_large = 1;
  f.cancel_at_small2 = 4;        /* 2 revert check, 3 and 4 second wait */
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_LOST_ANSWERED);
  CHECK(f.back_n == 1 && f.small_n == 4);
  CHECK(f.mode == AMIGA_RATE_ABORT_OFF);
}

/* Cancel seen between two large probes (the probe in flight was not
 * aborted): the next one is not tried. */
static void rate_cancel_between_large(void)
{
  fake_t f; fake_init(&f);
  f.fn_new_at = 1;
  f.fn_old_at = 2;
  f.cancel_at_large = 2;
  f.use_abortable = 0;           /* so the hook never aborts it */
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_CANCELLED);
  CHECK(f.large_n == 2 && f.set_n == 2 && f.save_n == 0);
}

static void rate_set_failed(void)
{
  fake_t f; fake_init(&f);
  f.set_ret[0] = FN_ERR_IO;
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_SET_FAILED);
  CHECK(f.set_n == 1 && f.small_n == 0 && f.abort_calls == 0);
}

static void rate_revert_not_abortable(void)
{
  fake_t f; fake_init(&f);
  int i;
  /* timeout */
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_TIMED_OUT);
  CHECK(f.small_n >= 2);
  for (i = 0; i < f.small_n - 1; i++) CHECK(f.small_mode[i] != 0);
  CHECK(f.small_mode[f.small_n - 1] == 0);
  CHECK(f.set_mode[1] == 0);

  /* cancel */
  fake_init(&f);
  f.cancel_at_small = 2;
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_CANCELLED);
  CHECK(f.small_n == 3);
  CHECK(f.small_mode[0] != 0 && f.small_mode[1] != 0 && f.small_mode[2] == 0);
  CHECK(f.set_mode[1] == 0);
}

/* The large-phase revert turns the abort off before set_baud(old). */
static void rate_large_revert_not_abortable(void)
{
  fake_t f; fake_init(&f);
  f.fn_new_at = 1;
  f.large_fail_at = 1;
  f.fn_old_at = 2;
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_UNRELIABLE);
  CHECK(f.set_n == 2 && f.set_args[1] == OLD && f.set_mode[1] == 0);
  CHECK(f.small_mode[1] == 0);   /* the revert's own check */

  fake_init(&f);
  f.fn_new_at = 1;
  f.cancel_at_large = 2;
  f.fn_old_at = 2;
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_CANCELLED);
  CHECK(f.set_n == 2 && f.set_mode[1] == 0 && f.small_mode[1] == 0);
}

static void rate_abortable_success_and_null(void)
{
  fake_t f; fake_init(&f);
  f.fn_new_at = 1;
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_SAVED);
  CHECK(f.mode == 0);
  CHECK(f.small_mode[0] == AMIGA_RATE_ABORT_ANY);
  CHECK(f.large_mode[0] == AMIGA_RATE_ABORT_CANCEL);

  fake_init(&f);
  f.use_abortable = 0;
  CHECK(run(&f, OLD, NEW, 1) == AMIGA_RATE_TIMED_OUT);
  fake_init(&f);
  f.use_abortable = 0;
  f.fn_new_at = 1;
  CHECK(run(&f, OLD, NEW, 0) == AMIGA_RATE_SESSION);
}

static void rate_should_abort(void)
{
  fake_t f; fake_init(&f);
  amiga_rate_ops_t ops;
  ops.elapsed_secs = f_elapsed;
  ops.cancelled = f_cancelled;
  CHECK(amiga_rate_should_abort(&ops, &f, AMIGA_RATE_ABORT_ANY) == 0);
  f.clock = 59;
  CHECK(amiga_rate_should_abort(&ops, &f, AMIGA_RATE_ABORT_ANY) == 0);
  f.clock = 60;
  CHECK(amiga_rate_should_abort(&ops, &f, AMIGA_RATE_ABORT_ANY) == 1);
  CHECK(amiga_rate_should_abort(&ops, &f, AMIGA_RATE_ABORT_CANCEL) == 0);
  CHECK(amiga_rate_should_abort(&ops, &f, AMIGA_RATE_ABORT_OFF) == 0);
  f.clock = 0;
  f.cancel = 1;
  CHECK(amiga_rate_should_abort(&ops, &f, AMIGA_RATE_ABORT_ANY) == 1);
  CHECK(amiga_rate_should_abort(&ops, &f, AMIGA_RATE_ABORT_CANCEL) == 1);
  CHECK(amiga_rate_should_abort(&ops, &f, AMIGA_RATE_ABORT_OFF) == 0);
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
  CHECK_STR(out, "Rate 57600 not saved: set FujiNet to 38400 before reboot");
  amiga_rate_result_text(AMIGA_RATE_CANCELLED, 38400, 57600, out);
  CHECK_STR(out, "Cancelled; back to 38400");
  amiga_rate_result_text(AMIGA_RATE_UNRELIABLE, 38400, 115200, out);
  CHECK_STR(out, "115200 lost data on long replies; back to 38400");
  amiga_rate_result_text(AMIGA_RATE_LOST, 38400, 57600, out);
  CHECK_STR(out, "No answer at 57600 either: set the FujiNet back to 38400");
  amiga_rate_result_text(AMIGA_RATE_LOST_ANSWERED, 38400, 57600, out);
  CHECK_STR(out, "Set the FujiNet back: uart.baud 38400 then uart.save");
  amiga_rate_result_text(AMIGA_RATE_SET_FAILED, 38400, 57600, out);
  CHECK_STR(out, "Could not set the serial rate");
  amiga_rate_result_text(AMIGA_RATE_UNCHANGED, 38400, 38400, out);
  CHECK_STR(out, "Serial rate is already 38400");
  CHECK(amiga_rate_choices[0] == 9600 && amiga_rate_choices[4] == 115200);
}

/* Every status fits the 80-char status buffer at the longest rates. */
static void rate_text_fits(void)
{
  char out[256];
  int r;
  for (r = AMIGA_RATE_UNCHANGED; r <= AMIGA_RATE_SET_FAILED; r++) {
    amiga_rate_result_text((amiga_rate_result_t) r, 115200, 57600, out);
    CHECK(strlen(out) < 80);
    amiga_rate_result_text((amiga_rate_result_t) r, 57600, 115200, out);
    CHECK(strlen(out) < 80);
  }
}

/* Requester texts: whole within the requester's 8 lines of 50 columns. */
static void rate_ask_text(void)
{
  char out[AMIGA_RATE_ASK_MAX + 64];
  char rows[9][51];
  int a;
  for (a = AMIGA_RATE_ASK_NO_BOOT_LINE; a <= AMIGA_RATE_ASK_SESSION; a++) {
    amiga_rate_ask_text((amiga_rate_ask_t) a, 115200, 57600, out);
    CHECK(strlen(out) < AMIGA_RATE_ASK_MAX);
    CHECK(amiga_wrap_text(&rows[0][0], 51, 9, 50, out) <= 8);
    amiga_rate_ask_text((amiga_rate_ask_t) a, 57600, 115200, out);
    CHECK(amiga_wrap_text(&rows[0][0], 51, 9, 50, out) <= 8);
  }
  amiga_rate_ask_text(AMIGA_RATE_ASK_CONFIRM, 38400, 57600, out);
  CHECK(strstr(out, "type: uart.baud 57600 and uart.save.") != NULL);
  CHECK(strstr(out, "back to 38400") != NULL);
  CHECK(strstr(out, "FujiNet drives may not respond while waiting.") != NULL);
  amiga_rate_ask_text(AMIGA_RATE_ASK_SESSION, 38400, 57600, out);
  CHECK(strstr(out, "type: uart.baud 57600 (not uart.save).") != NULL);
  CHECK(strstr(out, "FujiNet drives may not respond while waiting.") != NULL);
  amiga_rate_ask_text(AMIGA_RATE_ASK_NO_WRITE, 38400, 57600, out);
  CHECK_STR(out, "S:FujiNet-Config cannot be written (disk write-protected "
            "or full), so the new rate would be lost at the next boot.");
  amiga_rate_ask_text(AMIGA_RATE_ASK_NO_BOOT_LINE, 38400, 57600, out);
  CHECK_STR(out, "This Amiga's startup does not read S:FujiNet-Config, so "
            "the new rate would be lost at the next boot.");
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
  rate_unreliable_answered();
  rate_cancel_large_answered();
  rate_deadline_in_large();
  rate_lost();
  rate_unreliable_no_second_wait();
  rate_second_wait();
  rate_lost_answered_timeout();
  rate_cancel_large_second_wait();
  rate_cancel_second_wait();
  rate_cancel_between_large();
  rate_set_failed();
  rate_revert_not_abortable();
  rate_large_revert_not_abortable();
  rate_abortable_success_and_null();
  rate_should_abort();
  rate_text();
  rate_text_fits();
  rate_ask_text();
}
