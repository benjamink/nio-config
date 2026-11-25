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
  AMIGA_RATE_LOST,           /* reverted, but no answer at the old rate either */
  AMIGA_RATE_SET_FAILED      /* SET_BAUD to the new rate failed; nothing changed */
} amiga_rate_result_t;

typedef struct {
  uint8_t (*set_baud)(uint32_t baud, void *ctx);   /* FN_OK on success */
  uint8_t (*probe_small)(void *ctx);  /* FN_OK, FN_ERR_ABORTED (hook aborted), or other error */
  uint8_t (*probe_large)(void *ctx);  /* one 512-byte round trip; same codes */
  uint32_t (*elapsed_secs)(void *ctx); /* since amiga_rate_change started */
  int (*cancelled)(void *ctx);         /* 1 once the user pressed Cancel */
  int (*save)(uint32_t baud, void *ctx); /* 1 = S:FujiNet-Config written */
  /* May be NULL.  The GUI wait hook aborts exchanges while
   * amiga_rate_should_abort() is true, which is also true at revert time.
   * amiga_rate_change calls abortable(1) once the new rate is set (before the
   * first small probe) and abortable(0) before the revert, and before the
   * save / session success path, so the revert's check probe is never aborted.
   * The glue must make the hook ignore the abort condition while off. */
  void (*abortable)(int on, void *ctx);
} amiga_rate_ops_t;

amiga_rate_result_t amiga_rate_change(const amiga_rate_ops_t *ops, void *ctx,
                                      uint32_t old_baud, uint32_t new_baud,
                                      int persist);
/* Status line for the result; out holds 80 chars. */
void amiga_rate_result_text(amiga_rate_result_t r, uint32_t old_baud,
                            uint32_t new_baud, char *out);
/* Config-side deadline test the GUI's wait hook uses: 1 = abort now. */
int amiga_rate_should_abort(const amiga_rate_ops_t *ops, void *ctx);

#ifdef __AMIGA__
/* Amiga glue (amiga_rate_dos.c).  get/set use Config's own
 * fujinet-nio.device handle: FN_OK or an FN_ERR_* code. */
uint8_t amiga_rate_get(uint32_t *baud);
uint8_t amiga_rate_set(uint32_t baud);
/* 1 if S:Startup-Sequence, S:User-Startup or S:StartupII runs the boot
 * line.  Never writes them. */
int amiga_rate_boot_line_present(void);
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
