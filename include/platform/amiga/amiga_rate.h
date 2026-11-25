#ifndef AMIGA_RATE_H
#define AMIGA_RATE_H

#include <stdint.h>

/* Config's serial rate change flow as a pure state machine.  The Amiga glue
 * (device handle, DOS file I/O, FujiNet probes, GUI hook) supplies the ops. */

#define AMIGA_RATE_COUNT 5
extern const uint32_t amiga_rate_choices[AMIGA_RATE_COUNT]; /* 9600..115200 */
#define AMIGA_RATE_WAIT_SECS 60U
#define AMIGA_RATE_LARGE_TRIES 3U

typedef enum {
  AMIGA_RATE_UNCHANGED,      /* new == old: nothing done */
  AMIGA_RATE_SAVED,          /* answered, large check passed, saved */
  AMIGA_RATE_SESSION,        /* answered, large check passed, persist == 0 */
  AMIGA_RATE_NOT_SAVED,      /* answered, large check passed, save failed: new rate kept */
  AMIGA_RATE_TIMED_OUT,      /* reverted */
  AMIGA_RATE_CANCELLED,      /* reverted */
  AMIGA_RATE_UNRELIABLE,     /* large check failed; reverted */
  AMIGA_RATE_LOST,           /* reverted before any answer at the new rate,
                                but no answer at the old rate either */
  AMIGA_RATE_LOST_ANSWERED,  /* answered at the new rate, then reverted, and
                                the FujiNet was not set back in the second wait */
  AMIGA_RATE_SET_FAILED      /* SET_BAUD to the new rate failed; nothing changed */
} amiga_rate_result_t;

/* abortable() settings: when the GUI's wait hook may abort an exchange in
 * flight (amiga_rate_should_abort). */
#define AMIGA_RATE_ABORT_OFF    0  /* never: the revert's own check, the save */
#define AMIGA_RATE_ABORT_ANY    1  /* Cancel or the 60 s deadline: waiting for
                                      the first answer at the new rate */
#define AMIGA_RATE_ABORT_CANCEL 2  /* Cancel only: the large check, and the
                                      second wait for the FujiNet to go back */

typedef struct {
  uint8_t (*set_baud)(uint32_t baud, void *ctx);   /* FN_OK on success */
  uint8_t (*probe_small)(void *ctx);  /* FN_OK, FN_ERR_ABORTED (hook aborted), or other error */
  uint8_t (*probe_large)(void *ctx);  /* one 512-byte round trip; same codes */
  uint32_t (*elapsed_secs)(void *ctx); /* since amiga_rate_change started, or
                                          since wait_back */
  int (*cancelled)(void *ctx);         /* 1 once the user pressed Cancel */
  int (*save)(uint32_t baud, void *ctx); /* 1 = S:FujiNet-Config written */
  /* May be NULL.  Sets when the GUI wait hook aborts exchanges
   * (AMIGA_RATE_ABORT_*; the hook calls amiga_rate_should_abort with it):
   * ANY once the new rate is set, CANCEL once the FujiNet answered there,
   * OFF before every set_baud back to the old rate (so the revert's check
   * is never aborted) and before the save / session success path, CANCEL
   * again during the second wait, and OFF before amiga_rate_change returns
   * from any path that turned it on. */
  void (*abortable)(int mode, void *ctx);
  /* The FujiNet answered at the new rate, the flow went back to the old one,
   * and the old rate does not answer: the user must set the FujiNet back.
   * The GUI shows "uart.baud <old_baud>" / "uart.save" in its wait view,
   * restarts the 60 s clock (elapsed_secs counts from 0 again) and forgets
   * an earlier Cancel, so a second Cancel ends the second wait. */
  void (*wait_back)(uint32_t old_baud, void *ctx);
} amiga_rate_ops_t;

amiga_rate_result_t amiga_rate_change(const amiga_rate_ops_t *ops, void *ctx,
                                      uint32_t old_baud, uint32_t new_baud,
                                      int persist);
/* Status line for the result; out holds 80 chars. */
void amiga_rate_result_text(amiga_rate_result_t r, uint32_t old_baud,
                            uint32_t new_baud, char *out);
/* The GUI wait hook's test for an exchange in flight, for the flow's
 * current abortable() mode: 1 = abort now.  OFF never aborts, CANCEL aborts
 * once Cancel was pressed, ANY also once the 60 s deadline passed. */
int amiga_rate_should_abort(const amiga_rate_ops_t *ops, void *ctx, int mode);

/* Requester texts for Change rate..., wrapped by the GUI's requester
 * (at most 8 lines of 50 columns).  out holds AMIGA_RATE_ASK_MAX chars. */
typedef enum {
  AMIGA_RATE_ASK_NO_BOOT_LINE,  /* "This session only" / Cancel */
  AMIGA_RATE_ASK_NO_WRITE,      /* "This session only" / Cancel */
  AMIGA_RATE_ASK_CONFIRM,       /* OK / Cancel; the rate is saved */
  AMIGA_RATE_ASK_SESSION        /* OK / Cancel; this session only */
} amiga_rate_ask_t;
#define AMIGA_RATE_ASK_MAX 256
void amiga_rate_ask_text(amiga_rate_ask_t ask, uint32_t old_baud,
                         uint32_t new_baud, char *out);

#ifdef __AMIGA__
/* Amiga glue (amiga_rate_dos.c).  get/set use Config's own
 * fujinet-nio.device handle: FN_OK or an FN_ERR_* code. */
uint8_t amiga_rate_get(uint32_t *baud);
uint8_t amiga_rate_set(uint32_t baud);
/* 1 if S:Startup-Sequence, S:User-Startup or S:StartupII runs the boot
 * line.  Never writes them. */
int amiga_rate_boot_line_present(void);
/* 1 if S:FujiNet-Config.new can be created (it is deleted again): the save
 * after a switch can work.  0 on a write-protected or full disk. */
int amiga_rate_can_save(void);
/* BAUD=<baud> into S:FujiNet-Config, other lines kept; 1 when written. */
int amiga_rate_save(uint32_t baud);
uint8_t amiga_rate_probe_small(void);
uint8_t amiga_rate_probe_large(void);
/* Call reset before amiga_rate_change (the next probe_large writes the
 * check pattern) and cleanup after it, with the wait hook off (deletes the
 * check key if one was written). */
void amiga_rate_probe_reset(void);
void amiga_rate_probe_cleanup(void);
#endif

#endif
