# TRS-80 4/4P config-nio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A C `CONFIG/CMD` for LS-DOS 6.3.1 on the Model 4/4P giving Hosts, Browse, Slots and Setup, shipped on the boot disk.

**Architecture:** C (z88dk) UI and shared portable state logic, over a small C-callable asm glue to the existing `nioclient` entry table (8000h) and LS-DOS SVCs. Source in this repo (`trs80/`, `src/platform/trs80/`); the boot repo (`~/dev/boot-trs80-4p`, "BOOT") builds and places the `/CMD` on the image.

**Tech Stack:** z88dk (zcc, z80asm), sjasmplus, Python unittest, MAME `trs80m4p`, the BOOT repo's Z80 interpreter and `FakeNio`.

**Spec:** `docs/superpowers/specs/2026-11-16-trs80-config-nio-design.md` (this repo). Read it first, including its Navigation table.

## Global Constraints

- ASCII-only 80x24; keys per the spec's Navigation table; BREAK cancels (no ESC key on the 4P); F1-F3 unused.
- State records identical to other platforms (`docs/state-model.md`): hosts text, `mappings` v1, slot catalogue, preferences, app-store namespace `config-nio`.
- Memory: program from 3000h; do not overlap the 8000h entry table, 9F00h-A5FFh buffers, driver at E000h, HIGH$ DFFFh. Reuse `nioclient`; do not link a second `nioclib`.
- Interrupts off during FujiNet exchanges; UART 19200 baud 8O1 already set by the time CONFIG talks (SERIAL/FUJIDRV does this; CONFIG must set it up itself if not -- see Task 3).
- Console I/O through LS-DOS SVCs, not hardware.
- Commit per task, never push, no `Co-authored-by`/Claude trailer lines (workspace AGENTS.md overrides any attribution reminder). Cross-repo: commit in each repo separately; no submodule URL changes.
- Both repos: run the cheapest failing test per task; before finishing run `python3 -m unittest discover tests` in BOOT and the host tests in this repo. `make check` in `fujinet-nio-lib` is only needed if that repo is touched (it should not be).
- Export `source "$NIO_WORKSPACE/scripts/env.sh"` before building.

## Review Focus

- `CONFIG/CMD` too big for the nearly full boot disk (HELP/CMD and DOS/HLP are already dropped): the image builder must fail loudly, not truncate.
- FujiNet absent or timing out at start: show an error and return to LS-DOS (`Ready`) with interrupts restored, never hang.
- URI/SSID/filter fields at their maximum length (255 for URIs): no overflow, no screen corruption.
- Host or folder with more than one page of entries (>20) and the paged "more" flag: SHIFT+Up/Down and edge paging stay consistent.
- BREAK pressed inside a dialog vs at top level: cancels the dialog only; quits only from the top level, and always leaves the screen cleared and the cursor restored.

---

### Task 1: Toolchain proof (hello /CMD)

**Files:**
- Create (this repo): `trs80/Makefile`, `trs80/hello.c`, `trs80/crt0.asm`
- Create (BOOT): `tools/zcc_cmd.py` (hex/binary to LMF via `trs80.lmf.build_lmf`), `tests/test_zcc_cmd.py`

**Interfaces:**
- Produces: `trs80/Makefile` target `build/config.bin` built with `zcc +z80 -startup=<custom crt> -zorg=0x3000 -create-app`; `tools/zcc_cmd.py: bin_to_lmf(path: str, load: int = 0x3000, entry: int = 0x3000) -> bytes`.
- Console primitives the C will use (defined in Task 2): none yet; hello calls the LS-DOS `@DSPLY` SVC (SVC 10) from inline asm.

- [ ] **Step 1: Write the failing test** `test_bin_to_lmf_roundtrip`: build an LMF from a 600-byte blob at 3000h with `bin_to_lmf`; `parse_lmf` gives one contiguous segment at 3000h equal to the blob and entry 3000h.
- [ ] **Step 2: Run** `python3 -m unittest tests.test_zcc_cmd -v`; expect FAIL (module missing).
- [ ] **Step 3: Implement `bin_to_lmf` in `tools/zcc_cmd.py`** (split in 240-byte blocks like `tool_cmd`).
- [ ] **Step 4: Run test; expect PASS.**
- [ ] **Step 5: In this repo write `hello.c`/`crt0.asm`** printing `hello` through `@DSPLY`, building to a raw binary at 3000h that returns to LS-DOS with `jp 402Dh`-style exit as the other BOOT tools do (copy the exit sequence from `fujinet/z80/fls.asm`).
- [ ] **Step 6: Add a MAME integration test** in BOOT `tests/test_mame_integration.py` (same style as the FMOUNT one): add the binary to a scratch image as `HELLO/CMD`, run `HELLO`, assert `hello` on screen. Expected: PASS (`MAME_TESTS=1 python3 -m unittest tests.test_mame_integration -k hello`).
- [ ] **Step 7: Commit** in each repo (`trs80: z88dk toolchain proof`, `tools: zcc_cmd bin_to_lmf`).

### Task 2: Console layer and widgets (C, host-testable)

**Files:**
- Create: `src/platform/trs80/trs80_console.h`, `trs80_console.c`, `trs80_console_asm.asm`, `src/platform/trs80/trs80_keys.h`
- Create: `src/platform/trs80/ui_widgets.c`, `.h` (tab bar, list pane with scroll/filter marker, text field, message line)
- Test: `tests/trs80/test_widgets.c` (gcc host build against a fake console), `tests/trs80/Makefile`

**Interfaces:**
- Produces:
  `void con_clear(void); void con_put(uint8_t row, uint8_t col, const char *s); void con_fill(uint8_t row, uint8_t col, uint8_t n, char c); void con_reverse(uint8_t on); int con_getkey(void);`
  `trs80_keys.h`: `KEY_UP KEY_DOWN KEY_LEFT KEY_RIGHT KEY_SUP KEY_SDOWN KEY_ENTER KEY_BREAK KEY_TAB KEY_BKSP` constants (values determined in MAME; see Step 1).
  `int field_edit(char *buf, uint8_t max, uint8_t row, uint8_t col, uint8_t width)` returns 1 accepted, 0 cancelled (BREAK).
  `void list_draw(const list_t *l); int list_key(list_t *l, int key)`.

- [ ] **Step 1: Capture key codes**: in MAME run a tiny `/CMD` that prints `@KBD` codes for Up, Down, Left, Right, SHIFT+Up, SHIFT+Down, ENTER, BREAK, TAB, BACKSPACE; record them as the constants and in `docs/trs80-config.md`.
- [ ] **Step 2: Write failing host tests** `test_field_edit_max_length` (typing 300 chars into a 255 field leaves 255 and a NUL), `test_field_edit_break_cancels`, `test_list_pages_with_shift_down` (50 entries, 20 visible: SHIFT+Down moves selection by 19 and clamps), `test_list_edge_paging_flag`.
- [ ] **Step 3: Run** `make -C tests/trs80 test`; expect FAIL.
- [ ] **Step 4: Implement widgets** against the `con_*` interface (the host test supplies a fake `con_*` that records a 80x24 buffer); implement `trs80_console_asm.asm` with `@DSP`/`@KBD`/`@CLS` SVCs and hide the cursor handling there.
- [ ] **Step 5: Run host tests; expect PASS.** Commit.

### Task 3: Transport adapter (`nio_*` from C) with UART setup

**Files:**
- Create: `src/platform/trs80/nio_glue.asm` (C-callable wrappers for 8000h entry points), `fnsvc_trs80.c`, `.h`
- Test (BOOT): `tests/test_config_transport_emulated.py` (Z80 interpreter + `FakeNio`)

**Interfaces:**
- Consumes: Task 1 build; the 8000h table (`nio_request`, `disk_info`, `disk_mount`, `disk_unmount`, `disk_list`, `file_list`).
- Produces (match `include/common/fnsvc.h` so shared code compiles): `int fnsvc_list_directory_flags(const char *uri, fnsvc_list_cb cb, void *ctx)`, `fnsvc_list_pages(...)` per `fnsvc.h`, plus `int nio_uart_init(void)` (19200 8O1, same init as `z80/serialcmd.asm`), `int nio_mount(uint8_t slot, const char *uri, uint8_t ro)`, `int nio_unmount(uint8_t slot)`, `int nio_slot_info(uint8_t slot, slot_info_t *out)`.
- `CONFIG` must load `nioclient` if not resident: decide in this task whether CONFIG carries its own copy at 8000h (preferred, as FMOUNT does) and record the choice in the design's Open points.

- [ ] **Step 1: Failing test** `test_list_directory_through_fake_nio` (emulated Z80 program calls `fnsvc_list_directory_flags("sd0:/")`; asserts the callback sequence equals `FakeNio`'s folder: dirs first, sizes preserved) and `test_timeout_returns_transport_error` (FakeNio silent: returns `FNSVC_ERR_TRANSPORT` in the one-wait-count mode, never loops).
- [ ] **Step 2: Run** `python3 -m unittest tests.test_config_transport_emulated`; expect FAIL.
- [ ] **Step 3: Implement the glue and adapter.**
- [ ] **Step 4: Run; PASS.** Commit in both repos.

### Task 4: State, store and tables on the Z80

**Files:**
- Create: `src/platform/trs80/config_nio_store_trs80.c`, `config_nio_state_trs80.c` (reuse `src/platform/portable/config_nio_tables.c`, `_browse.c`, `_state.c` if they compile in z88dk; otherwise write the 4P versions with the same function names and note it in the spec Open points)
- Test: `tests/trs80/test_state_host.c`; BOOT `tests/test_config_state_emulated.py`

**Interfaces:**
- Consumes: Task 3 adapter (`nio_request` for app-store get/put). Produces the functions the portable UI layer calls: whatever `include/config_nio.h` and `config_nio_state_embedded.h` declare (do not change them).

- [ ] **Step 1: Failing tests**: `test_hosts_roundtrip` (set 3 hosts, save, reload from FakeNio app-store; equal), `test_mapping_v1_compat` (a `mappings` v1 record produced by the existing Linux/MS-DOS tests loads unchanged -- reuse their fixture bytes), `test_catalogue_slot_numbering` (drive Dn uses catalogue slot n-1, per `docs/state-model.md`).
- [ ] **Step 2: Run; FAIL. Step 3: implement. Step 4: Run; PASS.** Use a reduced table size only if memory demands it, and record the limit in `docs/trs80-config.md`.
- [ ] **Step 5: Commit.**

### Task 5: Browse, Slots and mount/eject screens

**Files:**
- Create: `src/platform/trs80/config_nio_ui.c` (tab loop, Browse two-pane, Slots), `src/platform/trs80/main_trs80.c`
- Test: BOOT `tests/test_mame_integration.py::test_config_browse_mount`

**Interfaces:**
- Consumes: Tasks 2-4. Produces `int main(void)` (LS-DOS entry via the crt) returning to LS-DOS on quit.

- [ ] **Step 1: Failing MAME test**: boot the scratch image with `CONFIG/CMD`, run `CONFIG`, press `B`, DOWN, ENTER on a `.dsk` in `FakeNio`'s `sd0:/`; assert the status line `Mounted <name> on :1`, the Slots pane lists it, and `FakeNio` records a mount for slot 2. Then `X` ejects (FakeNio records unmount). Then BREAK returns to `LS-DOS Ready`.
- [ ] **Step 2: Run; FAIL.** (`MAME_TESTS=1 python3 -m unittest tests.test_mame_integration -k config_browse`)
- [ ] **Step 3: Implement** Browse (paged listing via `fnsvc_list_pages`, `F` filter, `R` reload, BKSP up), Slots (drive tiles `:0`-`:7`, ENTER/E picks, X ejects), mount via `nio_mount` after verifying/installing `FUJIDRV` on the target drive; on any FujiNet error show it on the message line and stay in the app.
- [ ] **Step 4: Run; PASS.** Commit.

### Task 6: Hosts and Setup screens

**Files:**
- Modify: `src/platform/trs80/config_nio_ui.c`
- Test: BOOT `tests/test_mame_integration.py::test_config_hosts_edit`, `::test_config_setup_wifi`

- [ ] **Step 1: Failing MAME tests**: `A` adds host `tnfs://example/` via the text field, it persists across quitting and re-running `CONFIG` (FakeNio app-store); `D` deletes it; in Setup, Wi-Fi scan lists fake SSIDs, selecting one with a typed password issues the join request and shows success/failure from FakeNio.
- [ ] **Step 2: Run; FAIL. Step 3: Implement Hosts (A/E/D, ENTER browses) and Setup (status, IP/gateway/DNS/MAC/firmware, Wi-Fi scan/join, preferences per `config_nio_ui.c` in MS-DOS as the reference). If `FakeNio` lacks Wi-Fi commands, extend it in BOOT with the protocol from `repos/fujinet-nio` docs rather than stubbing the test.
- [ ] **Step 4: Run; PASS.** Commit.

### Task 7: Boot image integration, size guard and docs

**Files:**
- Modify (BOOT): `tools/make_boot_image.py` (add `CONFIG/CMD` from `tools/zcc_cmd.bin_to_lmf`), `Makefile` (build target calling this repo's `trs80/Makefile`), `tests/test_make_boot_image.py`, `README.md` (table + section)
- Create (this repo): `docs/trs80-config.md`; modify `README.md` target list; `Makefile` target `trs80` (opt-in, not in `all-targets`)

- [ ] **Step 1: Failing test** `test_boot_image_contains_config` (the built image lists `CONFIG/CMD`, still bootable) and `test_boot_image_fails_when_config_does_not_fit` (an oversized fake `CONFIG/CMD` makes `make_boot_image` raise with a message naming the file and free space; no output file written).
- [ ] **Step 2: Run; FAIL. Step 3: Implement. Step 4: Run all of BOOT `python3 -m unittest discover tests` and the host tests here; PASS.**
- [ ] **Step 5: Docs**: `docs/trs80-config.md` documents screens, keys (copied from the spec), memory map, key codes found in Task 2, and "not yet verified on a real 4P". Move nothing to `completed/`; record the checkbox in the workspace `backlog/` per AGENTS.md if a goal file exists.
- [ ] **Step 6: Commit** in each repo; report the real-4P check as pending for the owner.
