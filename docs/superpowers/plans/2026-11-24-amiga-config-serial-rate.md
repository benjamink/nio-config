# Amiga Config Serial Rate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let Config's Configuration window change the Amiga's RS-232 rate, wait for the user to set the FujiNet to match, save it to `S:FujiNet-Config` on success and revert on timeout or Cancel; boot disks apply the saved rate with `C:fujinet-nio-baud FROM=S:FujiNet-Config`.

**Architecture:** A pure parser/rewriter for `S:FujiNet-Config` lives in `fujinet-nio-driver/amiga/common/` and is compiled into both `fujinet-nio-baud` and Config. Config's change flow is a pure state machine (`amiga_rate.c`) driven through an ops table, so it is host-tested; Amiga-only glue (`amiga_rate_dos.c`) supplies the device handle, DOS file I/O and FujiNet probes. `fujinet-nio-lib` gains an optional wait hook so Config can show "Cancelling..." while an exchange is in flight.

**Tech Stack:** C99, m68k-amigaos-gcc (libnix nix13 runtime, 2.0 NDK headers), host gcc tests, Python 3 + xdftool (boot disk builder), AmigaDOS scripts.

**Spec:** `repos/nio-config/docs/superpowers/specs/2026-11-24-amiga-config-serial-rate-design.md`

## Global Constraints

- Settings file path `S:FujiNet-Config`; format one `KEY=value` per line, `;` comment lines, blank lines ignored, keys upper case; this work defines only `BAUD=`.
- Valid rates 300..230400. Config offers exactly 9600, 19200, 38400, 57600, 115200.
- Default shipped file content: `; FujiNet boot settings, written by Config\nBAUD=38400\n`.
- Boot line, exactly: `C:fujinet-nio-baud FROM=S:FujiNet-Config`.
- `fujinet-nio-baud FROM=`: file missing → silent, return 0; bad/missing `BAUD=` → one line, return 5; any other failure → one line, return 5. Never 20. Existing forms unchanged.
- Config never writes `S:Startup-Sequence`, `S:User-Startup` or `S:StartupII`; it only reads them.
- Wait 60 s; large-reply check: write a 512-byte pattern to app store namespace `config-nio`, key `rate-check`, read back 3 times, compare, delete.
- Everything runs on Kickstart 1.3: no ReadArgs, no V36+ calls without a version check, `Rename()` does not overwrite.
- Commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. A settings file edited on a PC (CRLF line endings, trailing spaces, no final newline) must still give its rate. Test in Task 2.
2. A boot line that is commented out (`; C:fujinet-nio-baud FROM=S:FujiNet-Config`) must not count as present. Test in Task 2.
3. Write-protected or full boot disk: saving fails after a successful switch; the new rate stays for the session and the status says it was not saved. Test in Task 7 (`save` op returns 0).
4. Rate passes the small probe but fails the large one (57600/115200 on a 68000): revert, nothing saved. Test in Task 7.
5. Cancel pressed during the large-reply check (not only the small probe): revert. Test in Task 7.

## Branches

- `repos/nio-config`: new branch `feat/amiga-config-serial-rate` from `feat/amiga-wb13-bootdisk` (after Task 1).
- `repos/fujinet-nio-driver` (detached at the workspace pin), `repos/fujinet-nio-lib` (`master`): new branch `feat/amiga-config-serial-rate` from the current HEAD.
- Workspace root (`fujinet-nio-workspace`, currently `pin/fujinet-nio-driver-serial-fix` with unrelated uncommitted files): new branch `feat/amiga-config-serial-rate`; stage only the files named in Task 3 and Task 6.

---

### Task 1: Commit the pending 38400 boot disk default

**Files (already modified, uncommitted, in `repos/nio-config`):** `Makefile`, `amiga/bootdisk/profiles/free.toml`, `docs/amiga-bootdisk.md`, `tests/amiga/bootdisk/test_build.py`. Leave the untracked file `-` alone.

- [ ] **Step 1: Run the boot disk tests**

Run: `cd repos/nio-config && make test-amiga-bootdisk`
Expected: `Ran 10 tests` … `OK`

- [ ] **Step 2: Commit on `feat/amiga-wb13-bootdisk`, then branch**

```bash
git add Makefile amiga/bootdisk/profiles/free.toml docs/amiga-bootdisk.md tests/amiga/bootdisk/test_build.py
git commit -m "Amiga free boot disk: default the RS-232 rate to 38400"
git switch -c feat/amiga-config-serial-rate
```

---

### Task 2: Settings file parser and rewriter (driver repo)

**Files:**
- Create: `repos/fujinet-nio-driver/amiga/include/fujinet_boot_config.h`
- Create: `repos/fujinet-nio-driver/amiga/common/fujinet_boot_config.c`
- Create: `repos/fujinet-nio-driver/amiga/tests/test_fujinet_boot_config.c`
- Modify: `repos/fujinet-nio-driver/amiga/tests/Makefile` (new `BOOT_CONFIG_TEST`, added to `all` and `test`)

**Interfaces:**
- Produces (header, no Amiga includes, `<stdint.h>` only):

```c
#define FUJINET_BOOT_CONFIG_PATH "S:FujiNet-Config"
#define FUJINET_BOOT_CONFIG_MAX  1024U   /* largest file the tools read */
#define FUJINET_BOOT_CONFIG_DEFAULT \
    "; FujiNet boot settings, written by Config\nBAUD=38400\n"
enum {
    FUJINET_BOOT_CONFIG_OK = 0,
    FUJINET_BOOT_CONFIG_NO_KEY,     /* no BAUD= line */
    FUJINET_BOOT_CONFIG_BAD_VALUE   /* not digits, or outside 300..230400 */
};
int fujinet_boot_config_get_baud(const char *text, uint16_t len, uint32_t *baud);
/* Writes `in` with the first BAUD= line replaced (later BAUD= lines dropped,
 * every other line copied unchanged, line endings normalised to LF), or
 * BAUD= appended if none; empty input gives FUJINET_BOOT_CONFIG_DEFAULT's
 * comment line then BAUD=<baud>. Returns bytes written, 0 if it does not fit
 * `cap`. Output always ends in "\n". */
uint16_t fujinet_boot_config_set_baud(const char *in, uint16_t in_len,
                                      uint32_t baud, char *out, uint16_t cap);
/* 1 if a non-comment line runs fujinet-nio-baud with FROM=S:FujiNet-Config
 * (case-insensitive, optional C: prefix, any spacing); else 0. */
int fujinet_boot_config_startup_uses(const char *text, uint16_t len);
```

- [ ] **Step 1: Write the failing tests** in `test_fujinet_boot_config.c`, CHECK macro style of `test_fujinet_paula_uart.c`:

```c
uint32_t b; char out[256]; uint16_t n;
CHECK("get-basic", fujinet_boot_config_get_baud("BAUD=38400\n", 11, &b) == 0 && b == 38400UL);
CHECK("get-crlf-trailing-space-no-eol",
      fujinet_boot_config_get_baud("; x\r\nBAUD=57600  \r\n", 19, &b) == 0 && b == 57600UL);
CHECK("get-no-final-newline", fujinet_boot_config_get_baud("BAUD=9600", 9, &b) == 0 && b == 9600UL);
CHECK("get-first-wins", fujinet_boot_config_get_baud("BAUD=19200\nBAUD=9600\n", 21, &b) == 0 && b == 19200UL);
CHECK("get-comment-ignored", fujinet_boot_config_get_baud(";BAUD=9600\n", 11, &b) == FUJINET_BOOT_CONFIG_NO_KEY);
CHECK("get-missing", fujinet_boot_config_get_baud("SERIAL=x\n", 9, &b) == FUJINET_BOOT_CONFIG_NO_KEY);
CHECK("get-bad", fujinet_boot_config_get_baud("BAUD=fast\n", 10, &b) == FUJINET_BOOT_CONFIG_BAD_VALUE);
CHECK("get-low", fujinet_boot_config_get_baud("BAUD=299\n", 9, &b) == FUJINET_BOOT_CONFIG_BAD_VALUE);
CHECK("get-high", fujinet_boot_config_get_baud("BAUD=230401\n", 12, &b) == FUJINET_BOOT_CONFIG_BAD_VALUE);
n = fujinet_boot_config_set_baud("; c\nSERIAL=x\nBAUD=38400\nBAUD=1\n", 32, 57600UL, out, sizeof(out));
CHECK("set-replace-keep-others", n && strncmp(out, "; c\nSERIAL=x\nBAUD=57600\n", n) == 0);
n = fujinet_boot_config_set_baud("SERIAL=x", 8, 9600UL, out, sizeof(out));
CHECK("set-append", n && strncmp(out, "SERIAL=x\nBAUD=9600\n", n) == 0);
n = fujinet_boot_config_set_baud("", 0, 38400UL, out, sizeof(out));
CHECK("set-empty-gives-default", n == strlen(FUJINET_BOOT_CONFIG_DEFAULT) &&
      strncmp(out, FUJINET_BOOT_CONFIG_DEFAULT, n) == 0);
CHECK("set-too-small", fujinet_boot_config_set_baud("BAUD=38400\n", 11, 115200UL, out, 8) == 0);
CHECK("startup-yes", fujinet_boot_config_startup_uses(
      "C:fujinet-load-resident x\nC:fujinet-nio-baud FROM=S:FujiNet-Config\n", 67) == 1);
CHECK("startup-case-noprefix", fujinet_boot_config_startup_uses(
      "  FujiNet-NIO-Baud  from=s:fujinet-config\n", 43) == 1);
CHECK("startup-commented", fujinet_boot_config_startup_uses(
      "; C:fujinet-nio-baud FROM=S:FujiNet-Config\n", 44) == 0);
CHECK("startup-fixed-rate", fujinet_boot_config_startup_uses(
      "C:fujinet-nio-baud 38400\n", 25) == 0);
```

(The length arguments above are hand-counted: write each call with `(uint16_t)strlen(literal)` instead.)

- [ ] **Step 2: Run and see it fail**

Run: `make -C repos/fujinet-nio-driver/amiga/tests test`
Expected: build error, `fujinet_boot_config.h: No such file`

- [ ] **Step 3: Implement the three functions** in `fujinet_boot_config.c`. Lines split on LF; a trailing CR and trailing spaces/tabs are stripped before matching; leading spaces/tabs are skipped; a line whose first non-blank character is `;` is a comment. `startup_uses` matches the command token by its last path component (`fujinet-nio-baud`, ignoring any `C:`/`SYS:C/` prefix) and requires a later token equal (case-insensitive) to `FROM=S:FujiNet-Config`.

- [ ] **Step 4: Run and see it pass**

Run: `make -C repos/fujinet-nio-driver/amiga/tests test`
Expected: no `FAIL:` lines, exit 0

- [ ] **Step 5: Commit** (driver repo, branch `feat/amiga-config-serial-rate`)

```bash
git add amiga/include/fujinet_boot_config.h amiga/common/fujinet_boot_config.c amiga/tests/test_fujinet_boot_config.c amiga/tests/Makefile
git commit -m "amiga: S:FujiNet-Config parser and rewriter"
```

---

### Task 3: `fujinet-nio-baud FROM=`, and the tool on Kickstart 1.3 packages

**Files:**
- Modify: `repos/fujinet-nio-driver/amiga/tools/fujinet-nio-baud.c`
- Modify: `repos/fujinet-nio-driver/amiga/Makefile` (`$(NIO_BAUD)` rule: add `common/fujinet_boot_config.c`, use `$(MOUNT_TOOL_CRT)` instead of `-mcrt=clib2`; move `$(NIO_BAUD)` out of the `ifneq ($(AMIGA_WB13),1)` block so wb13 builds it)
- Modify: `repos/nio-config/Makefile` (`amiga-bootdisk-tools`: add `"$(DRIVER_DIR)/amiga/common/fujinet_boot_config.c"` to the `fujinet-nio-baud` compile)
- Modify (workspace root): `scripts/amiga-artifacts` (build and stage `fujinet-nio-baud` for wb13 too), `configs/amiga/release-adf-wb13.yaml` (list `build/amiga-artifacts/wb13/NIO/fujinet-nio-baud`)
- Modify: `repos/fujinet-nio-driver/amiga/README.md` (RS-232 section: document `FROM=` and the file format)

**Interfaces:**
- Consumes: `fujinet_boot_config_get_baud`, `FUJINET_BOOT_CONFIG_MAX` (Task 2).
- Produces: command form `fujinet-nio-baud FROM=<file>` (argument prefix `FROM=` matched case-insensitively; usage text becomes `fujinet-nio-baud [300..230400 | FROM=<file>]`).

- [ ] **Step 1: Implement the `FROM=` form.** Read up to `FUJINET_BOOT_CONFIG_MAX` bytes with `Open(MODE_OLDFILE)`/`Read`/`Close`. Missing file (Open fails): return `RETURN_OK` silently. Parse result not OK: print `fujinet-nio-baud: no valid BAUD= in <file>` and return `RETURN_WARN`. Device open or `SET_BAUD` failure in this form: same messages as today, return `RETURN_WARN`. On success print the existing `FujiNet NIO baud: <n>` line.

- [ ] **Step 2: Build every profile that ships it**

Run: `scripts/amiga-artifacts wb13 && scripts/amiga-artifacts wb32 && make -C repos/nio-config amiga-bootdisk-tools`
Expected: all three succeed; `ls build/amiga-artifacts/wb13/NIO/fujinet-nio-baud repos/nio-config/build/amiga-bootdisk-tools/fujinet-nio-baud` lists both.

- [ ] **Step 3: Commit** (driver repo; workspace root on its branch, staging only the two workspace files; nio-config Makefile is committed with Task 5)

```bash
git add amiga/tools/fujinet-nio-baud.c amiga/Makefile amiga/README.md
git commit -m "fujinet-nio-baud: FROM=<file> reads BAUD= and never fails a boot"
```

---

### Task 4: Wait hook in `fujinet-nio-lib`

**Files:**
- Modify: `repos/fujinet-nio-lib/include/fujinet-nio.h` (inside the existing `#ifdef __AMIGA__` block near line 899)
- Modify: `repos/fujinet-nio-lib/src/platform/amiga/fn_transport.c` (`fn_transport_exchange_buffers`)
- Modify: `repos/fujinet-nio-lib/tests/amiga_transport_host_test.c` and `tests/amiga_transport_stubs/proto/exec.h` (add `SendIO`, `CheckIO`, `Wait` stubs; `Wait` returns a scripted sequence of masks)

**Interfaces:**
- Produces:

```c
/* While an exchange is in flight, each time a signal in `signals` arrives
 * the library calls hook(ctx).  Nonzero aborts the request (AbortIO, then
 * WaitIO) and the call returns FN_ERR_ABORTED with no response.  hook NULL
 * or signals 0 restores plain DoIO.  One hook per process. */
typedef int (*fn_amiga_wait_hook_t)(void *ctx);
void fn_amiga_set_wait_hook(uint32_t signals, fn_amiga_wait_hook_t hook, void *ctx);
```

- [ ] **Step 1: Write the failing tests** in `amiga_transport_host_test.c`:
  - `hook-unset-uses-doio`: no hook → `g_doio_count == 1`, no `SendIO`.
  - `hook-reply-first`: hook set, `Wait` returns the port bit and `CheckIO` reports done → hook never called, result `FN_OK`, `g_abort_count == 0`.
  - `hook-keeps-waiting`: `Wait` returns the hook signal twice (hook returns 0), then the port bit → hook called 2 times, result `FN_OK`.
  - `hook-aborts`: hook returns 1 on the first signal → `g_abort_count == 1`, `g_wait_count == 1`, result `FN_ERR_ABORTED`, `*response_length == 0`.

- [ ] **Step 2: Run and see it fail**

Run: `repos/fujinet-nio-lib/tests/run_amiga_transport_host_test.sh`
Expected: compile error, `fn_amiga_set_wait_hook` undeclared

- [ ] **Step 3: Implement.** With a hook set: `SendIO`, then loop `got = Wait(portmask | signals)`; break when `CheckIO(&req.fn_io)`; if `(got & signals) && hook(ctx)` then `AbortIO`, mark aborted, break. Always `WaitIO` before reading the request. Aborted → `FN_ERR_ABORTED` (existing code `0x13`).

- [ ] **Step 4: Run and see it pass** (same command). Expected: all checks pass, including the existing ones.

- [ ] **Step 5: Cross-build the Amiga library**

Run: `make -C repos/fujinet-nio-lib amiga`
Expected: builds with no warnings.

- [ ] **Step 6: Commit**

```bash
git add include/fujinet-nio.h src/platform/amiga/fn_transport.c tests/amiga_transport_host_test.c tests/amiga_transport_stubs/proto/exec.h
git commit -m "amiga: optional wait hook so a caller can abort an exchange in flight"
```

---

### Task 5: Boot disks apply `S:FujiNet-Config`

**Files:**
- Modify: `repos/nio-config/amiga/bootdisk/build.py` (new optional profile key `boot_config`, written to `S/FujiNet-Config` exactly like `startup_sequence`, in the tuple at line ~232)
- Modify: `repos/nio-config/amiga/bootdisk/profiles/free.toml`, `wb204.toml`, `wb13.toml`
- Modify: `repos/nio-config/tests/amiga/bootdisk/test_build.py`
- Modify: `repos/nio-config/Makefile` (from Task 3)

**Interfaces:**
- Consumes: the boot line and default content from Global Constraints; `fujinet-nio-baud` in the wb13 and wb32 packages and the bootdisk tools (Task 3).

Profile changes:
- `free`: startup line `C:fujinet-nio-baud 38400` → `C:fujinet-nio-baud FROM=S:FujiNet-Config` (keep its comment, reworded to say the rate comes from `S:FujiNet-Config` and Config changes it); add `boot_config`.
- `wb204`: package `fujinet-nio-baud` → `C/fujinet-nio-baud`; append the boot line to `user_startup` after the `fujinet-disk.device` line; add `boot_config`.
- `wb13`: package `fujinet-nio-baud` → `c/fujinet-nio-baud`; add the boot line to the `s/StartupII` `insert` list after the two `fujinet-load-resident` lines; add `boot_config`.

- [ ] **Step 1: Write the failing tests.** Replace `test_free_profile_sets_38400_before_config` with:

```python
BOOT_LINE = "C:fujinet-nio-baud FROM=S:FujiNet-Config"
DEFAULT = "; FujiNet boot settings, written by Config\nBAUD=38400\n"

def test_profiles_apply_boot_config(self):
    for name in ("free", "wb204", "wb13"):
        prof = build.load_profile(name)
        self.assertEqual(prof["boot_config"], DEFAULT, name)
        dsts = [p["dst"].lower() for p in prof["package"]]
        self.assertIn("c/fujinet-nio-baud", dsts, name)
    free = build.load_profile("free")["startup_sequence"].splitlines()
    self.assertLess(free.index(BOOT_LINE), free.index("Tools/config-nio"))
    self.assertNotIn("C:fujinet-nio-baud 38400", free)
    wb204 = build.load_profile("wb204")["user_startup"].splitlines()
    self.assertLess(wb204.index("C:fujinet-load-resident DEVS:fujinet-disk.device fujinet-disk.device"),
                    wb204.index(BOOT_LINE))
    ins = build.load_profile("wb13")["edit"][0]["insert"]
    self.assertEqual(ins[-1], BOOT_LINE)
```

and extend `test_boot_block_checksum_and_scratch_build` (the scratch build) to give its synthetic profile `"boot_config": DEFAULT` and assert `S/FujiNet-Config` is on the built disk with that content (read back with `xdf(out, "read", "S/FujiNet-Config", path)`). Add `"fujinet-nio-baud"` to the allowed set in `test_free_profiles_ship_no_commodore_files` (already there from Task 1).

- [ ] **Step 2: Run and see it fail**

Run: `make -C repos/nio-config test-amiga-bootdisk`
Expected: FAIL on `boot_config` KeyError / missing line

- [ ] **Step 3: Implement** the `build.py` key and the three profile edits.

- [ ] **Step 4: Run and see it pass** (same command). Expected: `OK`.

- [ ] **Step 5: Build the free disk and check it**

Run: `make -C repos/nio-config amiga-bootdisk WB=free && xdftool repos/nio-config/build/amiga-bootdisk/FujiNet-Free-Boot.adf type S/FujiNet-Config`
Expected: the two-line default file.

- [ ] **Step 6: Commit**

```bash
git add Makefile amiga/bootdisk/build.py amiga/bootdisk/profiles/free.toml amiga/bootdisk/profiles/wb204.toml amiga/bootdisk/profiles/wb13.toml tests/amiga/bootdisk/test_build.py
git commit -m "Amiga boot disks: apply the rate saved in S:FujiNet-Config"
```

---

### Task 6: Installers

**Files:**
- Modify: `repos/nio-config/amiga/bootdisk/wb204/Install-HD`
- Modify: `repos/nio-config/amiga/install/Install-config-nio`
- Modify (workspace root): `configs/amiga/install/Install-FujiNet-WB13`, `scripts/amiga-artifacts` (stage `FujiNet-Config` with the default content into every profile's `NIO/`), `configs/amiga/release-adf-wb13.yaml`, `release-adf-wb31.yaml`, `release-adf-wb32.yaml` (list it)

Each installer, in its existing style:
- copies `fujinet-nio-baud` into `C:` next to the other FujiNet commands;
- copies a default `S:FujiNet-Config` only `If NOT EXISTS` (source: `SYS:S/FujiNet-Config` on the wb204 floppy; `{FROM}FujiNet-Config` for Install-config-nio, so the wb31/wb32 staging in `scripts/amiga-artifacts` must also install `FujiNet-Config` with the default content; `NIO:FujiNet-Config` for WB1.3, staged the same way for wb13);
- Install-HD and Install-config-nio: append the boot line to `S:User-Startup` with the existing `Search >NIL: … / If WARN / Echo >>` guard (search string `FROM=S:FujiNet-Config`), after the device lines;
- Install-FujiNet-WB13: add `Echo "C:fujinet-nio-baud FROM=S:FujiNet-Config"` to the lines it tells the user to put in `S:StartupII`, after the two `fujinet-load-resident` lines.

- [ ] **Step 1: Make the edits.**
- [ ] **Step 2: Check the scripts in Amiberry** if the workspace's Amiberry harness is available (`integration-tests/amiberry`, WB 3.1 and WB 1.3 cases); otherwise record "not run" for the hardware checklist in Task 9. Expected: a fresh install leaves `C:fujinet-nio-baud`, `S:FujiNet-Config` and one boot line; running it twice adds no second line and keeps an edited `S:FujiNet-Config`.
- [ ] **Step 3: Commit** (nio-config files; workspace files on the workspace branch together with Task 3's two files)

```bash
git add amiga/bootdisk/wb204/Install-HD amiga/install/Install-config-nio
git commit -m "Amiga installers: install fujinet-nio-baud and S:FujiNet-Config"
```

---

### Task 7: Config rate change state machine (host-tested)

**Files:**
- Create: `repos/nio-config/include/platform/amiga/amiga_rate.h`
- Create: `repos/nio-config/src/platform/amiga/amiga_rate.c`
- Create: `repos/nio-config/tests/amiga/test_rate.c`; add `AMIGA_TEST(rate)` to `tests/amiga/tests.def`
- Modify: `repos/nio-config/makefiles/test-amiga-host.mk` (add `src/platform/amiga/amiga_rate.c` and `$(FUJINET_NIO_DRIVER)/amiga/common/fujinet_boot_config.c` to the sources, `-I$(FUJINET_NIO_DRIVER)/amiga/include` to `TEST_CFLAGS`, `FUJINET_NIO_DRIVER ?= ../fujinet-nio-driver`)

**Interfaces:**
- Produces:

```c
#define AMIGA_RATE_COUNT 5
extern const uint32_t amiga_rate_choices[AMIGA_RATE_COUNT]; /* 9600 19200 38400 57600 115200 */
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
} amiga_rate_ops_t;

amiga_rate_result_t amiga_rate_change(const amiga_rate_ops_t *ops, void *ctx,
                                      uint32_t old_baud, uint32_t new_baud,
                                      int persist);
/* Status line for the result; out holds 80 chars. */
void amiga_rate_result_text(amiga_rate_result_t r, uint32_t old_baud,
                            uint32_t new_baud, char *out);
/* Config-side deadline test the GUI's wait hook uses: 1 = abort now. */
int amiga_rate_should_abort(const amiga_rate_ops_t *ops, void *ctx);
```

Flow: `set_baud(new)` (fail → `SET_FAILED`); loop `probe_small` until `FN_OK`, `cancelled()` (→ revert, `CANCELLED`) or `elapsed_secs() >= 60` (→ revert, `TIMED_OUT`); then `probe_large` 3 times (any non-OK: `cancelled()` → `CANCELLED`, else → `UNRELIABLE`, both revert); then `persist ? save : SESSION`. Revert = `set_baud(old)` then one `probe_small`; non-OK → `LOST`. `amiga_rate_should_abort` = `cancelled() || elapsed_secs() >= 60`.

Status texts (exact copy):
- SAVED: `Serial rate %lu saved`
- SESSION: `Serial rate %lu for this session only`
- NOT_SAVED: `Serial rate %lu in use but not saved (disk full or write-protected?)`
- TIMED_OUT: `No answer at %lu; back to %lu`
- CANCELLED: `Cancelled; back to %lu`
- UNRELIABLE: `%lu lost data on long replies; back to %lu`
- LOST: `No answer at %lu either: set the FujiNet back to %lu`
- SET_FAILED: `Could not set the serial rate`
- UNCHANGED: `Serial rate is already %lu`

- [ ] **Step 1: Write the failing tests** in `test_rate.c` with a scripted fake ops table (arrays of return codes, a fake clock that advances 5 s per failed small probe and 0 s per success, counters for each op, the list of `set_baud` arguments). One case each:
  - `rate_unchanged`: old == new → `UNCHANGED`, no `set_baud` call.
  - `rate_saved`: small OK first try, large OK ×3, persist 1 → `SAVED`, `set_baud` calls `{57600}`, save called once with 57600.
  - `rate_session`: same, persist 0 → `SESSION`, save never called.
  - `rate_not_saved` (Review Focus 3): save returns 0 → `NOT_SAVED`, no revert (`set_baud` calls `{57600}`).
  - `rate_timeout`: small always fails → `TIMED_OUT`, `set_baud` calls `{57600, 38400}`, save never called.
  - `rate_cancel_small`: cancelled after 2 failed probes, probe returns `FN_ERR_ABORTED` → `CANCELLED`, reverted.
  - `rate_unreliable` (Review Focus 4): small OK, large fails on try 2 → `UNRELIABLE`, reverted, save never called.
  - `rate_cancel_large` (Review Focus 5): small OK, large returns `FN_ERR_ABORTED` with cancelled → `CANCELLED`, reverted.
  - `rate_lost`: timeout, and the probe after reverting fails → `LOST`.
  - `rate_set_failed`: first `set_baud` fails → `SET_FAILED`, no probes.
  - `rate_text`: `amiga_rate_result_text(AMIGA_RATE_TIMED_OUT, 38400, 57600, out)` gives `No answer at 57600; back to 38400`, and one CHECK_STR per remaining result with the texts above.

- [ ] **Step 2: Run and see it fail**

Run: `make -C repos/nio-config test-amiga-host ONLY=rate`
Expected: compile error, `amiga_rate.h` not found

- [ ] **Step 3: Implement** `amiga_rate.c` (no Amiga headers; `amiga_sprintf` from `amiga_fmt.h` for the texts).

- [ ] **Step 4: Run the whole host suite**

Run: `make -C repos/nio-config test-amiga-host`
Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add include/platform/amiga/amiga_rate.h src/platform/amiga/amiga_rate.c tests/amiga/test_rate.c tests/amiga/tests.def makefiles/test-amiga-host.mk
git commit -m "Amiga Config: serial rate change flow"
```

---

### Task 8: "Serial rate" row on the Device tab

**Files:**
- Modify: `repos/nio-config/include/platform/amiga/amiga_ctl.h` (`AMIGA_NET_ROWS` 12 → 13; `amiga_net_t` gains `uint32_t baud; uint8_t have_baud;`)
- Modify: `repos/nio-config/include/platform/amiga/amiga_net.h` (`AMIGA_NET_ROW_RATE` after `AMIGA_NET_ROW_PROFILE`)
- Modify: `repos/nio-config/src/platform/amiga/amiga_net.c` (label `"Serial rate"`; value `%lu` or `Unknown` when `!have_baud`)
- Modify: `repos/nio-config/tests/amiga/test_net.c`

**Interfaces:**
- Produces: `AMIGA_NET_ROW_RATE`, `amiga_net_t.baud`, `amiga_net_t.have_baud` (filled by Task 9's glue on Refresh).

- [ ] **Step 1: Write the failing test** in `test_net.c`:

```c
memset(&net, 0, sizeof(net));
amiga_net_row_text(&net, AMIGA_NET_ROW_RATE, text);
CHECK_STR(text, "Serial rate   Unknown");
net.have_baud = 1; net.baud = 38400;
amiga_net_row_text(&net, AMIGA_NET_ROW_RATE, text);
CHECK_STR(text, "Serial rate   38400");
CHECK(AMIGA_NET_DEVICE_ROWS == 3);
```

- [ ] **Step 2: Run and see it fail**

Run: `make -C repos/nio-config test-amiga-host ONLY=net`
Expected: `AMIGA_NET_ROW_RATE` undeclared

- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run the whole host suite** (`make -C repos/nio-config test-amiga-host`). Expected: pass.
- [ ] **Step 5: Commit**

```bash
git add include/platform/amiga/amiga_ctl.h include/platform/amiga/amiga_net.h src/platform/amiga/amiga_net.c tests/amiga/test_net.c
git commit -m "Amiga Config: Serial rate row on the Device tab"
```

---

### Task 9: Amiga glue, Configuration window views, help

**Files:**
- Create: `repos/nio-config/src/platform/amiga/amiga_rate_dos.c` (Amiga only, not in the host build); declare its functions in `amiga_rate.h` under `#ifdef __AMIGA__`
- Modify: `repos/nio-config/makefiles/build.mk` (`CONFIG_NIO_EXTRA_SRCS_amiga` += `$(SRC_DIR)/platform/amiga/amiga_rate.c $(SRC_DIR)/platform/amiga/amiga_rate_dos.c $(FUJINET_NIO_DRIVER)/amiga/common/fujinet_boot_config.c`; `FUJINET_NIO_DRIVER ?= ../fujinet-nio-driver`); `makefiles/compiler-amigagcc.mk` (`CFLAGS += -I$(FUJINET_NIO_DRIVER)/amiga/include`)
- Modify: `repos/nio-config/include/platform/amiga/amiga_ctl.h` (`AMIGA_NET_VIEW_RATE`, `AMIGA_NET_VIEW_RATE_WAIT` after `AMIGA_NET_VIEW_JOIN`)
- Modify: `repos/nio-config/src/platform/amiga/amiga_gui.c` (Configuration window)
- Modify: `repos/nio-config/src/platform/amiga/amiga_help.c` (Configuration topic) and `tests/amiga/test_help.c`

**Interfaces:**
- Consumes: `amiga_rate_change`, `amiga_rate_result_text`, `amiga_rate_should_abort`, `amiga_rate_choices` (Task 7); `fujinet_boot_config_*` (Task 2); `fn_amiga_set_wait_hook` (Task 4); `AMIGA_NET_ROW_RATE`, `amiga_net_t.baud/have_baud` (Task 8).
- Produces (`amiga_rate_dos.c`):

```c
uint8_t amiga_rate_get(uint32_t *baud);   /* own fujinet-nio.device handle, GET_BAUD */
uint8_t amiga_rate_set(uint32_t baud);    /* SET_BAUD */
int amiga_rate_boot_line_present(void);   /* reads S:Startup-Sequence, S:User-Startup, S:StartupII */
int amiga_rate_save(uint32_t baud);       /* read, fujinet_boot_config_set_baud, write S:FujiNet-Config.new, DeleteFile old, Rename */
uint8_t amiga_rate_probe_small(void);     /* fn_fuji_get_info */
uint8_t amiga_rate_probe_large(void);     /* app store config-nio/rate-check 512-byte round trip */
```

`amiga_rate_probe_large`: on its first call in a change, `fn_appstore_write` the 512-byte pattern `byte[i] = (uint8_t)(i ^ 0x5a)`; each call `fn_appstore_read`s 512 bytes and compares (mismatch → `FN_ERR_IO`); after the flow ends (any result) `fn_appstore_delete` the key, ignoring errors.

GUI behaviour (`amiga_gui.c`):
- Device view buttons become `{ "Refresh", CA_REFRESH }, { "Change rate...", CA_RATE_BEGIN }, …, { "Close", CA_CLOSE }`. Refresh also fills `have_baud`/`baud` via `amiga_rate_get`.
- `AMIGA_NET_VIEW_RATE`: the list shows the five rates (`  9600`, … with `*` before the current one); buttons `{ "Use", CA_RATE_USE }, { "Cancel", CA_CANCEL }, …, { "Close", CA_CLOSE }`. Double-click = Use.
- Use: if `amiga_rate_boot_line_present()` is 0, a two-choice requester: `This Amiga's startup does not read S:FujiNet-Config,\nso the new rate would be lost at the next boot.` buttons `This session only` / `Cancel`; persist = 0 if chosen. Otherwise the confirm requester with the copy from the spec (`Config switches the Amiga to %lu now. Then, on the FujiNet's console,\ntype: uart.baud %lu and uart.save. Config waits 60 seconds and switches\nback to %lu if the FujiNet does not answer.`), OK / Cancel.
- `AMIGA_NET_VIEW_RATE_WAIT`: rows show `On the FujiNet's console type:`, `  uart.baud <new>`, `  uart.save`, blank, `Waiting: <n> s left` (or `Cancelling...` once Cancel is pressed); only button `{ "Cancel", CA_RATE_CANCEL }`. While in this view the window adds `IDCMP_INTUITICKS`; `fn_amiga_set_wait_hook(window signal, hook, ctx)` with a hook that drains IDCMP, sets the cancelled flag on the Cancel gadget, redraws the countdown, and returns `amiga_rate_should_abort()`. The hook is cleared (`fn_amiga_set_wait_hook(0, NULL, NULL)`) and `IDCMP_INTUITICKS` removed when the flow returns. `elapsed_secs` uses `CurrentTime()` (1.3-safe).
- After the flow: back to the Device view, Refresh, status line = `amiga_rate_result_text(...)`.
- Help (`amiga_help.c`, Configuration topic): add a paragraph naming the Serial rate row, Change rate..., the FujiNet console commands `uart.baud <rate>` and `uart.save`, the 60-second wait and revert, and `S:FujiNet-Config`. Add a `test_help.c` CHECK that the Configuration topic text contains `uart.baud`.

- [ ] **Step 1: Write the failing help test**, run `make -C repos/nio-config test-amiga-host ONLY=help`, expect FAIL.
- [ ] **Step 2: Implement** `amiga_rate_dos.c`, the views, the hook and the help text.
- [ ] **Step 3: Host suite** — `make -C repos/nio-config test-amiga-host` → pass.
- [ ] **Step 4: Cross-build Config for both profiles**

Run: `make -C repos/nio-config amiga AMIGA_PROFILE=wb13 AMIGA_WB13=1 && make -C repos/nio-config amiga AMIGA_PROFILE=wb32`
Expected: both link; no new warnings.

- [ ] **Step 5: Commit**

```bash
git add src/platform/amiga/amiga_rate_dos.c include/platform/amiga/amiga_rate.h include/platform/amiga/amiga_ctl.h src/platform/amiga/amiga_gui.c src/platform/amiga/amiga_help.c tests/amiga/test_help.c makefiles/build.mk makefiles/compiler-amigagcc.mk
git commit -m "Amiga Config: change the serial rate from the Configuration window"
```

---

### Task 10: Docs and hardware check

**Files:**
- Modify: `repos/nio-config/docs/amiga-bootdisk.md` (replace "change the `C:fujinet-nio-baud 38400` line" with Config's Serial rate setting and `S:FujiNet-Config`; document the file format and that Workbench disks now default to 38400)
- Modify: `repos/nio-config/docs/amiga-config.md` (Configuration window: the Serial rate row and Change rate...)

- [ ] **Step 1: Edit the docs.**
- [ ] **Step 2: Rebuild the free disk** (`make -C repos/nio-config amiga-bootdisk WB=free`) and hand it to the user for the hardware check:
  1. Boot on KS 1.3 (PiStorm with Z2 RAM) at 38400: Device tab shows `Serial rate 38400`.
  2. Change rate... → 19200 → set the FujiNet (`uart.baud 19200`, `uart.save`) within 60 s → status `Serial rate 19200 saved`; reboot; Config connects at 19200.
  3. Change rate... → 38400, do nothing → after 60 s `No answer at 38400; back to 19200`, still connected.
  4. Change rate... → 38400, press Cancel → `Cancelling...` at once, then `Cancelled; back to 19200` within ~5 s.
  5. Change rate... → 57600 on KS 1.3 with the FujiNet set to 57600 → expect `57600 lost data on long replies; back to …`; set the FujiNet back as told.
  6. Restore 38400 on both ends.
- [ ] **Step 3: Commit**

```bash
git add docs/amiga-bootdisk.md docs/amiga-config.md
git commit -m "Docs: Amiga serial rate setting and S:FujiNet-Config"
```
