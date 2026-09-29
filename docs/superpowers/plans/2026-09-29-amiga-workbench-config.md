# Amiga Workbench config-nio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an Amiga Workbench (Kickstart/Workbench 1.3 and later) build of
`config-nio`. It gives Amiga users the host, browse, catalogue and drive-mapping
functions of the BBC and MS-DOS config programs, presented as a normal Intuition
window.

**Architecture:** A new `TARGET=amiga` reuses the portable state/store/service
layers (`config_nio_state.c`, `config_nio_store.c`, `config_nio_tables.c`,
`fnsvc.c`). Amiga behaviour sits in a pure-C controller (`amiga_ctl`) plus small
pure helpers: list scrolling, layout, theme pens, key mapping, formatting,
options, drive endpoints and a script driver. All of these are unit-tested on the
host with gcc against an in-memory fake of the fujinet-nio calls. A thin Intuition
layer (`amiga_gui.c`) and the process glue (`amiga_main.c`, `amiga_exec.c`) are
the only Amiga-only code. They use the V33 (1.3) Intuition API throughout, with
optional V36+ pens. Drive insert and eject run the existing `FMOUNT`/`FUMOUNT`
commands, so the resident-device lifecycle that `nio-core-apps` owns is not
duplicated.

**Tech Stack:** C99. Target build uses `m68k-amigaos-gcc` (clib2 for wb31/wb32,
nix13 for wb13) and `fujinet-nio-amiga.a`. Host tests use gcc. The guest
acceptance test uses the workspace Amiberry pytest harness.

**Spec:** This plan is the spec. It is based on the user request (2026-09-29):
"new fujinet-nio config program for the Amiga Workbench (version >= 1.3) …
similar functionality to the existing bbc & msdos config programs … designed to
fit common Amiga aesthetics & workflow patterns … tests before code … no changes
to the current bbc or msdos config code … additive". The Design section below
records the decisions the plan makes on the user's behalf.

## Design

### What the user gets (feature parity map)

| BBC / MS-DOS feature | Amiga equivalent |
| --- | --- |
| Hosts list: add, edit, delete, move, browse | **Hosts** page: list + `URI` string gadget; buttons Browse / Add / Replace / Remove / Move Up / Move Down |
| Browse host, enter directory, up, assign file to slot | **Browse** page: file list with name, size (or `Drawer`) and date; Open / Parent / Refresh / Assign; `Slot` integer gadget and `RO` checkbox choose the target |
| Slots page: page through slots 0–255, edit, clear | **Catalogue** page: scrolling list of all 256 slots; selecting a slot loads its URI and RO flag into the edit gadgets; Set / Clear |
| Drive map: map drive to slot RO/RW, clear, "Mount + Exit" | **Drives** page: DN0:–DN7: (WB1.3: DN0: DN1: HN0: HN1: DO0: DO1: HO0: HO1:); Insert runs `FMOUNT slot drive RO/RW`, Eject runs `FUMOUNT drive`. Both take effect immediately, so there is no separate mount-and-exit step |
| Preferences (date/size format, colours) | **Settings** menu: dates `YY-MM-DD`/`YY-DD-MM` and sizes Full/Compact as mutually exclusive check items, saved at once. Colours follow the Workbench palette (Amiga convention), so the DOS colour editor is not ported |
| Status line | Recessed status line at the bottom of the window |

### Amiga look and workflow decisions

- One fixed-size window on the Workbench screen with a drag bar, depth gadget
  and close gadget. It is centred, opens active and is titled `FujiNet Config`.
  Topaz 8 is used for rendering, so the layout is identical on 1.3 and 3.x.
- A row of four page buttons (Hosts / Browse / Catalogue / Drives) stands in
  for tabs, which 1.3 lacks. The selected page button is drawn recessed and filled.
- Lists are drawn by the program: a recessed bevel box, a proportional
  scroller on the right, click to select, double-click to activate, and cursor
  keys (Shift = page, Alt = top/bottom). Scrolling with the scroller does not
  move the selection, which is standard Amiga listview behaviour.
- Bevels use DrawInfo pens on V36+ (`SHINEPEN`/`SHADOWPEN`/`FILLPEN`…). On 1.3
  they use the fixed Workbench pens: 1 white, 2 black, 3 orange.
- Menus: **Project** (`About...` RAmiga-?, `Quit` RAmiga-Q) and **Settings**
  (date and size check items). Esc quits; Help shows About; Tab and Shift-Tab
  change page.
- Destructive actions (Remove host, Clear slot, Eject) ask for confirmation
  in a classic `AutoRequest` with the positive choice on the left and the
  negative on the right.
- Long operations show the standard wait pointer and block the window with an
  empty `Requester`.
- Hosts, catalogue and drives are commit-on-action, as on DOS and BBC, because
  their data lives on the FujiNet. The Preferences-editor Save/Use/Cancel
  pattern is deliberately **not** used. It would suggest a staged edit that the
  FujiNet does not support.
- Starts from Shell (`config-nio [FMOUNT=path] [FUMOUNT=path] [SCRIPT=file]
  [RESULT=file]`) or from its Workbench icon, whose ToolTypes use the same
  `KEY=value` syntax. Unknown ToolTypes are ignored, which is the Workbench
  convention.
- `SCRIPT=` feeds text commands to the same controller the gadgets use and
  writes a transcript to `RESULT=`. It exists because the Amiberry harness
  cannot inject mouse input. It is how the guest test drives the real binary
  inside its real window.

### Mounting contract (do not duplicate nio-core-apps)

`fujinet-disk.device` persists `config-nio/mappings` when a unit is mounted,
and `FMOUNT`/`FUMOUNT` own DOS-node creation and handler retirement
(`docs/amiga/disk-media-architecture.md`). config-nio therefore **never** writes
`mappings` itself on the Amiga, and never calls the portable
`config_nio_mount_mappings()`. It works like `FMOUNTRESTORE`:

1. Close its FujiNet session (`fn_shutdown()`).
2. Run the command: `SystemTags` on V36+, `Execute` on 1.3.
3. Reopen the session (`fn_init()`).
4. Reload the state and check that the mapping appeared (insert) or
   disappeared (eject).

Step 4 is the success test. The return code is reported, but it is not trusted
alone, because KS1.3 `Execute()` cannot return a command's return code.

## Global Constraints

- Runtime floor: Kickstart/Workbench 1.3 (intuition/graphics/icon.library V33). V36+ calls only behind `#ifndef __KICK13__` **and** a runtime `lib_Version >= 36` check.
- Use old-style (1.3) Intuition constant names (`GADGHCOMP`, `RELVERIFY`, `BOOLGADGET`, `STRGADGET`, `PROPGADGET`, `TOGGLESELECT`, `LONGINT`, `AUTOKNOB`, `FREEVERT`, `WINDOWDRAG`, …), which exist in both the 1.3 and 3.x NDK headers.
- Amiga CRTs: `AMIGA_CRT=clib2` for wb31/wb32, `AMIGA_CRT=nix13` for wb13 (as `scripts/amiga-artifacts`). CPU flags `-mcpu=68000 -msoft-float`.
- Default Shell STACK is 4096. The program declares `long __stack = 16384;` (`docs/amiga/cli-stack-and-iorequest.md`).
- Command tools default to `SYS:C/fmount` and `SYS:C/fumount` (same path as `FMOUNTRESTORE`).
- `CONFIG_NIO_MAX_ENTRIES` for Amiga and host tests: 200. Every other target keeps 20.
- No edits to anything under `src/platform/bbc/`, `src/platform/msdos/`, `src/platform/portable/`, `src/platform/linux/`, `bbc/`, `msdos/`, `integration-tests/beebium/`, or `src/main.c`.
- Only these shared files may be edited, and only additively: `Makefile`, `makefiles/targets.mk`, `makefiles/build.mk`, `include/config_nio_state_embedded.h` (a value-preserving `#ifndef` guard), `README.md`, `RUNNING_TESTS.md`. `linux` and `bbc` binaries must stay byte-identical to the pre-change baseline (Task 0 / Task 16).
- `amiga` is **not** added to the top-level `TARGETS`/`all-targets`. Machines without the Amiga toolchain keep working unchanged.
- Commit messages: describe the change and its verification. **No `Co-authored-by`/`Co-Authored-By` trailer** (workspace `AGENTS.md` overrides harness attribution). Never push.
- All work happens on branch `amiga-workbench-config` in `repos/nio-config`. Workspace-owned harness changes (Task 15) go on a same-named branch of the workspace repo.
- Source the environment before builds: `source "$NIO_WORKSPACE/scripts/env.sh"` (or `source ../../scripts/env.sh` from `repos/nio-config`).

## Review Focus

1. **Empty and edge-sized lists.** Examples: no hosts, an empty or failed directory, 256 catalogue rows, removing the last host. Nothing should crash, select-dependent buttons should report "No host selected" or "Nothing selected", and the scroller should show a full-size knob. Pinned in Task 2 (`test_list`) and Task 7 (`test_ctl_hosts`).
2. **FMOUNT/FUMOUNT failures, including the silent failures on KS1.3.** The status must name the drive and rc, and must never report success when the mapping did not change. Pinned in Task 10 (`test_ctl_drives`).
3. **Directories larger than the entry table and over-long names** (`CONFIG_NIO_ENTRY_FLAG_NAME_TRUNCATED`). The list shows the first 200 entries with "Showing 200 of N entries". A truncated name cannot be opened or assigned. Pinned in Task 8 (`test_ctl_browse`) and Task 3 (`test_format` clipping).
4. **Screens that are small (NTSC 640×200) or have a tall title bar / large screen font (WB3.x).** The window must fit or refuse clearly. It must never draw off-window. Pinned in Task 6 (`test_layout`), with a visual check on KS1.3, KS2.04 and WB3.2 in Task 15.
5. **Network failure while browsing** (host unreachable). The page stays usable and the status shows `Browse failed: error E status S`. Pinned in Task 8 (`test_ctl_browse`).

---

## File Structure

```
repos/nio-config/
  Makefile                                  (modify: amiga, test-amiga-host targets)
  makefiles/targets.mk                      (modify: amiga target table rows)
  makefiles/build.mk                        (modify: amiga source list, main.c filter)
  makefiles/compiler-amigagcc.mk            (create: m68k-amigaos-gcc rules)
  makefiles/test-amiga-host.mk              (create: host test build/run)
  include/config_nio_state_embedded.h       (modify: #ifndef guard on MAX_ENTRIES)
  include/platform/amiga/
    amiga_drives.h   amiga_list.h   amiga_format.h  amiga_options.h
    amiga_theme.h    amiga_input.h  amiga_layout.h  amiga_ctl.h
    amiga_script.h   amiga_exec.h   amiga_gui.h
  src/platform/amiga/
    amiga_drives.c   drive endpoint labels, FMOUNT/FUMOUNT command text   (pure)
    amiga_list.c     list selection/scroll/prop maths                     (pure)
    amiga_format.c   size/date text, clipping                             (pure)
    amiga_options.c  CLI args and ToolTypes                               (pure)
    amiga_theme.c    pen roles for 1.3 and DrawInfo                       (pure)
    amiga_input.c    RAWKEY -> action                                     (pure)
    amiga_layout.c   gadget rectangles from borders and fonts             (pure)
    amiga_ctl.c      controller over config_nio state/store               (pure)
    amiga_script.c   SCRIPT= command interpreter                          (pure)
    amiga_exec.c     run FMOUNT/FUMOUNT (Execute/SystemTags)              (Amiga)
    amiga_gui.c      Intuition window, gadgets, menus, rendering          (Amiga)
    amiga_main.c     main(), libraries, Workbench/ToolTypes, fatal requester (Amiga)
    amiga_stack.c    __stack                                              (Amiga)
  tests/amiga/
    check.h  tests.def  run_tests.c  fake_nio.h  fake_nio.c
    test_drives.c test_list.c test_format.c test_options.c test_theme.c
    test_input.c test_layout.c test_ctl_hosts.c test_ctl_browse.c
    test_ctl_catalogue.c test_ctl_drives.c test_script.c
  amiga/
    gfx/config-nio.icon.txt   tools/mkinfo.py   icons/config-nio.info
  docs/amiga-config.md                      (create: user + developer guide)
```

`linux/fnctl.c` is platform-neutral (app-store + `fn_raw_call`), so the Amiga
build links it directly rather than copying it.

---

### Task 0: Baseline

**Files:** none (records evidence only)

- [ ] **Step 1: Confirm branch and clean tree**

Run:
```sh
cd "$NIO_WORKSPACE/repos/nio-config"
git status --short && git rev-parse --abbrev-ref HEAD
```
Expected: no output from status, and branch `amiga-workbench-config`.

- [ ] **Step 2: Build and keep baseline binaries for the shared-file regression check**

Run:
```sh
source ../../scripts/env.sh
make TARGET=linux FUJINET_NIO_LIB=../fujinet-nio-lib
make TARGET=bbc FUJINET_NIO_LIB=../fujinet-nio-lib
mkdir -p ../../build/nio-config-baseline
cp build/linux/bin/config-nio ../../build/nio-config-baseline/config-nio.linux
cp build/bbc/bin/config-nio   ../../build/nio-config-baseline/config-nio.bbc
```
Expected: both builds succeed. If either toolchain is unavailable, record the exact target and missing tool in the task review, and skip that target's comparison in Task 16.

- [ ] **Step 3: Confirm the Amiga toolchain and driver SDK**

Run:
```sh
m68k-amigaos-gcc --version | head -1
../../scripts/amiga-artifacts wb32 >/dev/null && ls ../fujinet-nio-driver/build/amiga/wb32/include ../fujinet-nio-driver/build/amiga/wb32/lib
```
Expected: gcc version line, plus `include` and `lib` directories. `amiga-artifacts wb32` also builds `fujinet-nio-amiga.a`.

---

### Task 1: Host test harness + drive endpoints

**Files:**
- Create: `makefiles/test-amiga-host.mk`, `tests/amiga/check.h`, `tests/amiga/tests.def`, `tests/amiga/run_tests.c`, `tests/amiga/test_drives.c`
- Create: `include/platform/amiga/amiga_drives.h`, `src/platform/amiga/amiga_drives.c`
- Modify: `Makefile` (add `test-amiga-host` target)

**Interfaces:**
- Produces:
  - `const char *amiga_drive_label(uint8_t unit, uint8_t kick13)` returns `"DN0:"`…, or `NULL` when `unit >= 8`.
  - `int amiga_drive_unit(const char *label, uint8_t kick13)` returns 0–7, or -1.
  - `int amiga_fmount_command(char *out, uint16_t cap, const char *fmount, uint8_t slot, uint8_t unit, uint8_t kick13, uint8_t readonly)` and `int amiga_fumount_command(char *out, uint16_t cap, const char *fumount, uint8_t unit, uint8_t kick13)` return 1 on success, 0 on failure.
  - Constants `AMIGA_DRIVE_COUNT 8`, `AMIGA_DEFAULT_FMOUNT "SYS:C/fmount"`, `AMIGA_DEFAULT_FUMOUNT "SYS:C/fumount"`.
  - Test macros `CHECK(cond)` and `CHECK_STR(actual, expected)`. Registering a test means adding `AMIGA_TEST(name)` to `tests/amiga/tests.def` and defining `void test_name(void)`.

Background: WB2+ `FMOUNT` takes `DN0:`–`DN7:`, and WB2+ `FUMOUNT` takes a **bare digit** (`Usage: FUMOUNT 0|...|7`). The WB1.3 builds of both take the eight static endpoint labels: unit 0–7 = `DN0: DN1: HN0: HN1: DO0: DO1: HO0: HO1:`.

- [ ] **Step 1: Write the harness**

`tests/amiga/check.h`:
```c
#ifndef AMIGA_TEST_CHECK_H
#define AMIGA_TEST_CHECK_H

#include <stdio.h>
#include <string.h>

extern int amiga_test_failures;
extern int amiga_test_checks;

#define CHECK(cond) do { \
  amiga_test_checks++; \
  if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    amiga_test_failures++; \
  } \
} while (0)

#define CHECK_STR(actual, expected) do { \
  const char *a_ = (actual); \
  const char *e_ = (expected); \
  amiga_test_checks++; \
  if (!a_ || strcmp(a_, e_) != 0) { \
    printf("FAIL %s:%d: %s == \"%s\" (got \"%s\")\n", __FILE__, __LINE__, \
           #actual, e_, a_ ? a_ : "(null)"); \
    amiga_test_failures++; \
  } \
} while (0)

#endif
```

`tests/amiga/tests.def`:
```c
AMIGA_TEST(drives)
```

`tests/amiga/run_tests.c`:
```c
/*
 * Host-side tests for the Amiga config-nio logic.  Run with
 * `make test-amiga-host` (all) or `make test-amiga-host ONLY=<name>`.
 */
#include "check.h"

int amiga_test_failures;
int amiga_test_checks;

#define AMIGA_TEST(name) void test_##name(void);
#include "tests.def"
#undef AMIGA_TEST

int main(int argc, char **argv)
{
  const char *only = argc > 1 && argv[1][0] ? argv[1] : NULL;

#define AMIGA_TEST(name) \
  if (!only || strcmp(only, #name) == 0) { \
    printf("-- %s\n", #name); \
    test_##name(); \
  }
#include "tests.def"
#undef AMIGA_TEST

  printf("amiga host tests: %d checks, %d failures\n",
         amiga_test_checks, amiga_test_failures);
  return amiga_test_failures ? 1 : 0;
}
```

`makefiles/test-amiga-host.mk`:
```make
# Host-side (gcc) tests for the Amiga config-nio logic.  Needs no Amiga
# toolchain and no FujiNet: tests/amiga/fake_nio.c stands in for the
# fujinet-nio library calls used by the portable store and the controller.
HOSTCC ?= cc
FUJINET_NIO_LIB ?= ../fujinet-nio-lib
BUILD_DIR ?= build
TEST_DIR := $(BUILD_DIR)/test-amiga-host
TEST_BIN := $(TEST_DIR)/run_tests

TEST_CFLAGS := -std=c99 -Wall -Wextra -O0 -g \
	-Iinclude -Iinclude/common -Iinclude/platform/amiga -Itests/amiga \
	-I$(FUJINET_NIO_LIB)/include \
	-DFNSVC_LIST_MAX_PAYLOAD=420

AMIGA_HOST_SRCS := \
	src/platform/amiga/amiga_drives.c
PORTABLE_HOST_SRCS :=
TEST_SUPPORT_SRCS := tests/amiga/run_tests.c
TEST_SRCS := $(wildcard tests/amiga/test_*.c)

.PHONY: all
all: $(TEST_BIN)
	$(TEST_BIN) $(ONLY)

$(TEST_BIN): $(TEST_SUPPORT_SRCS) $(TEST_SRCS) $(AMIGA_HOST_SRCS) $(PORTABLE_HOST_SRCS) \
		$(wildcard tests/amiga/*.h tests/amiga/*.def include/platform/amiga/*.h include/*.h) \
		makefiles/test-amiga-host.mk | $(TEST_DIR)
	$(HOSTCC) $(TEST_CFLAGS) -o $@ $(filter %.c,$^)

$(TEST_DIR):
	mkdir -p $@
```

Add to the top-level `Makefile`. Append to its `.PHONY` list and place the rule after `clean`:
```make
test-amiga-host:
	$(MAKE) -f makefiles/test-amiga-host.mk FUJINET_NIO_LIB=$(or $(FUJINET_NIO_LIB),../fujinet-nio-lib) ONLY=$(ONLY)
```

- [ ] **Step 2: Write the failing test** `tests/amiga/test_drives.c`

```c
#include "check.h"
#include "amiga_drives.h"

void test_drives(void)
{
  char buf[64];

  CHECK_STR(amiga_drive_label(0, 0), "DN0:");
  CHECK_STR(amiga_drive_label(7, 0), "DN7:");
  CHECK_STR(amiga_drive_label(2, 1), "HN0:");
  CHECK_STR(amiga_drive_label(4, 1), "DO0:");
  CHECK_STR(amiga_drive_label(7, 1), "HO1:");
  CHECK(amiga_drive_label(8, 0) == NULL);

  CHECK(amiga_drive_unit("dn3:", 0) == 3);
  CHECK(amiga_drive_unit("DN3", 0) == 3);
  CHECK(amiga_drive_unit("DN3:", 1) == -1);
  CHECK(amiga_drive_unit("ho0:", 1) == 6);
  CHECK(amiga_drive_unit("DN0::", 0) == -1);
  CHECK(amiga_drive_unit("", 0) == -1);
  CHECK(amiga_drive_unit(NULL, 0) == -1);

  CHECK(amiga_fmount_command(buf, sizeof(buf), "SYS:C/fmount", 12, 0, 0, 1));
  CHECK_STR(buf, "SYS:C/fmount 12 DN0: RO");
  CHECK(amiga_fmount_command(buf, sizeof(buf), "SYS:C/fmount", 255, 3, 1, 0));
  CHECK_STR(buf, "SYS:C/fmount 255 HN1: RW");
  CHECK(amiga_fumount_command(buf, sizeof(buf), "SYS:C/fumount", 5, 0));
  CHECK_STR(buf, "SYS:C/fumount 5");
  CHECK(amiga_fumount_command(buf, sizeof(buf), "SYS:C/fumount", 5, 1));
  CHECK_STR(buf, "SYS:C/fumount DO1:");

  /* A path with a space would be split by the command line parser. */
  CHECK(!amiga_fmount_command(buf, sizeof(buf), "Work:My Tools/fmount", 1, 0, 0, 0));
  CHECK(!amiga_fmount_command(buf, 10, "SYS:C/fmount", 1, 0, 0, 0));
  CHECK(!amiga_fmount_command(buf, sizeof(buf), "SYS:C/fmount", 1, 8, 0, 0));
  CHECK(!amiga_fumount_command(buf, sizeof(buf), "", 0, 0));
}
```

- [ ] **Step 3: Run it to verify it fails**

Run: `make test-amiga-host ONLY=drives`
Expected: compile error, `amiga_drives.h: No such file or directory`.

- [ ] **Step 4: Implement**

`include/platform/amiga/amiga_drives.h`:
```c
#ifndef AMIGA_DRIVES_H
#define AMIGA_DRIVES_H

#include <stdint.h>

#define AMIGA_DRIVE_COUNT 8
#define AMIGA_DEFAULT_FMOUNT "SYS:C/fmount"
#define AMIGA_DEFAULT_FUMOUNT "SYS:C/fumount"

const char *amiga_drive_label(uint8_t unit, uint8_t kick13);
int amiga_drive_unit(const char *label, uint8_t kick13);
int amiga_fmount_command(char *out, uint16_t cap, const char *fmount,
                         uint8_t slot, uint8_t unit, uint8_t kick13,
                         uint8_t readonly);
int amiga_fumount_command(char *out, uint16_t cap, const char *fumount,
                          uint8_t unit, uint8_t kick13);

#endif
```

`src/platform/amiga/amiga_drives.c`:
```c
#include "amiga_drives.h"

#include <stdio.h>
#include <string.h>

static const char *const labels_v36[AMIGA_DRIVE_COUNT] = {
  "DN0:", "DN1:", "DN2:", "DN3:", "DN4:", "DN5:", "DN6:", "DN7:"
};

/* WB1.3 static MountList endpoints; the index is the fujinet-disk.device
 * unit (docs/amiga/disk-media-architecture.md, WB1.3 static-unit workflow). */
static const char *const labels_kick13[AMIGA_DRIVE_COUNT] = {
  "DN0:", "DN1:", "HN0:", "HN1:", "DO0:", "DO1:", "HO0:", "HO1:"
};

const char *amiga_drive_label(uint8_t unit, uint8_t kick13)
{
  if (unit >= AMIGA_DRIVE_COUNT)
    return NULL;
  return kick13 ? labels_kick13[unit] : labels_v36[unit];
}

static char upper(char c)
{
  return (char) ((c >= 'a' && c <= 'z') ? c - 32 : c);
}

int amiga_drive_unit(const char *label, uint8_t kick13)
{
  uint8_t unit;

  if (!label || !label[0])
    return -1;
  for (unit = 0; unit < AMIGA_DRIVE_COUNT; unit++) {
    const char *want;
    uint8_t i;

    want = amiga_drive_label(unit, kick13);
    i = 0;
    while (want[i] != ':' && upper(label[i]) == want[i])
      i++;
    if (want[i] == ':' &&
        (label[i] == 0 || (label[i] == ':' && label[i + 1] == 0)))
      return unit;
  }
  return -1;
}

static int path_ok(const char *path, uint16_t cap, uint16_t tail)
{
  const char *p;

  if (!path || !path[0])
    return 0;
  for (p = path; *p; p++) {
    if (*p == ' ' || *p == '"' || *p == '\n')
      return 0;
  }
  return (uint16_t) (strlen(path) + tail) < cap;
}

int amiga_fmount_command(char *out, uint16_t cap, const char *fmount,
                         uint8_t slot, uint8_t unit, uint8_t kick13,
                         uint8_t readonly)
{
  const char *label;

  label = amiga_drive_label(unit, kick13);
  /* " 255 DN0: RO" is at most 12 characters. */
  if (!out || !label || !path_ok(fmount, cap, 12))
    return 0;
  sprintf(out, "%s %u %s %s", fmount, (unsigned) slot, label,
          readonly ? "RO" : "RW");
  return 1;
}

int amiga_fumount_command(char *out, uint16_t cap, const char *fumount,
                          uint8_t unit, uint8_t kick13)
{
  const char *label;

  label = amiga_drive_label(unit, kick13);
  if (!out || !label || !path_ok(fumount, cap, 5))
    return 0;
  if (kick13)
    sprintf(out, "%s %s", fumount, label);
  else
    sprintf(out, "%s %u", fumount, (unsigned) unit);
  return 1;
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `make test-amiga-host ONLY=drives`
Expected: `amiga host tests: N checks, 0 failures`, exit 0.

- [ ] **Step 6: Commit**

```bash
git add Makefile makefiles/test-amiga-host.mk tests/amiga include/platform/amiga/amiga_drives.h src/platform/amiga/amiga_drives.c
git commit -m "amiga: add host test harness and drive endpoint commands

Verified with: make test-amiga-host ONLY=drives"
```

---

### Task 2: List model

**Files:**
- Create: `include/platform/amiga/amiga_list.h`, `src/platform/amiga/amiga_list.c`, `tests/amiga/test_list.c`
- Modify: `tests/amiga/tests.def` (add `AMIGA_TEST(list)`), `makefiles/test-amiga-host.mk` (add `src/platform/amiga/amiga_list.c` to `AMIGA_HOST_SRCS`)

**Interfaces:**
- Produces (`amiga_list.h`):
```c
#define AMIGA_LIST_NONE 0xFFFFu
typedef struct { uint16_t count; uint16_t top; uint16_t selected; uint8_t rows; } amiga_list_t;
void amiga_list_init(amiga_list_t *l, uint8_t rows);
void amiga_list_set_rows(amiga_list_t *l, uint8_t rows);
void amiga_list_set_count(amiga_list_t *l, uint16_t count);
void amiga_list_select(amiga_list_t *l, uint16_t index);
void amiga_list_move(amiga_list_t *l, int16_t delta);
int  amiga_list_hit(const amiga_list_t *l, int16_t y, uint8_t row_h, uint16_t *index);
void amiga_list_prop(const amiga_list_t *l, uint16_t *pot, uint16_t *body);
void amiga_list_set_top_from_pot(amiga_list_t *l, uint16_t pot);
```

- [ ] **Step 1: Write the failing test** `tests/amiga/test_list.c`

```c
#include "check.h"
#include "amiga_list.h"

void test_list(void)
{
  amiga_list_t l;
  uint16_t pot, body, idx;

  amiga_list_init(&l, 10);
  CHECK(l.selected == AMIGA_LIST_NONE && l.count == 0 && l.top == 0);
  amiga_list_move(&l, 1);
  CHECK(l.selected == AMIGA_LIST_NONE);
  CHECK(!amiga_list_hit(&l, 0, 9, &idx));
  amiga_list_prop(&l, &pot, &body);
  CHECK(pot == 0 && body == 0xFFFF);

  amiga_list_set_count(&l, 5);
  CHECK(l.selected == 0 && l.top == 0);

  amiga_list_set_count(&l, 100);
  amiga_list_move(&l, 15);
  CHECK(l.selected == 15 && l.top == 6);
  amiga_list_move(&l, -100);
  CHECK(l.selected == 0 && l.top == 0);
  amiga_list_select(&l, 99);
  CHECK(l.selected == 99 && l.top == 90);
  amiga_list_select(&l, 500);
  CHECK(l.selected == 99);
  amiga_list_prop(&l, &pot, &body);
  CHECK(body == 6553 && pot == 0xFFFF);

  /* Dragging the scroller moves the view, not the selection. */
  amiga_list_set_top_from_pot(&l, 0x7FFF);
  CHECK(l.top == 45 && l.selected == 99);

  CHECK(amiga_list_hit(&l, 0, 9, &idx) && idx == 45);
  CHECK(amiga_list_hit(&l, 89, 9, &idx) && idx == 54);
  CHECK(!amiga_list_hit(&l, 90, 9, &idx));
  CHECK(!amiga_list_hit(&l, -1, 9, &idx));

  amiga_list_select(&l, 99);
  amiga_list_set_count(&l, 50);
  CHECK(l.selected == 49 && l.top == 40);

  /* A hit below the last entry of a short list selects nothing. */
  amiga_list_set_count(&l, 3);
  CHECK(!amiga_list_hit(&l, 27, 9, &idx));

  amiga_list_set_rows(&l, 0);
  CHECK(l.rows == 1);

  amiga_list_set_count(&l, 0);
  CHECK(l.selected == AMIGA_LIST_NONE && l.top == 0);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test-amiga-host ONLY=list` (after adding `AMIGA_TEST(list)` to `tests.def`)
Expected: compile error, `amiga_list.h: No such file or directory`.

- [ ] **Step 3: Implement**

`include/platform/amiga/amiga_list.h`: the declarations in **Interfaces** above, with `#include <stdint.h>` and an include guard `AMIGA_LIST_H`.

`src/platform/amiga/amiga_list.c`:
```c
#include "amiga_list.h"

static void clamp_top(amiga_list_t *l)
{
  uint16_t max_top;

  max_top = l->count > l->rows ? (uint16_t) (l->count - l->rows) : 0;
  if (l->top > max_top)
    l->top = max_top;
}

static void ensure_visible(amiga_list_t *l)
{
  if (l->selected == AMIGA_LIST_NONE)
    return;
  if (l->selected < l->top)
    l->top = l->selected;
  else if (l->selected >= (uint16_t) (l->top + l->rows))
    l->top = (uint16_t) (l->selected - l->rows + 1);
}

void amiga_list_init(amiga_list_t *l, uint8_t rows)
{
  l->count = 0;
  l->top = 0;
  l->selected = AMIGA_LIST_NONE;
  l->rows = rows ? rows : 1;
}

void amiga_list_set_rows(amiga_list_t *l, uint8_t rows)
{
  l->rows = rows ? rows : 1;
  clamp_top(l);
  ensure_visible(l);
}

void amiga_list_set_count(amiga_list_t *l, uint16_t count)
{
  l->count = count;
  if (count == 0)
    l->selected = AMIGA_LIST_NONE;
  else if (l->selected == AMIGA_LIST_NONE)
    l->selected = 0;
  else if (l->selected >= count)
    l->selected = (uint16_t) (count - 1);
  clamp_top(l);
  ensure_visible(l);
}

void amiga_list_select(amiga_list_t *l, uint16_t index)
{
  if (l->count == 0)
    return;
  l->selected = index >= l->count ? (uint16_t) (l->count - 1) : index;
  ensure_visible(l);
}

void amiga_list_move(amiga_list_t *l, int16_t delta)
{
  int32_t target;

  if (l->count == 0)
    return;
  target = (int32_t) (l->selected == AMIGA_LIST_NONE ? 0 : l->selected) + delta;
  if (target < 0)
    target = 0;
  if (target >= (int32_t) l->count)
    target = (int32_t) l->count - 1;
  amiga_list_select(l, (uint16_t) target);
}

int amiga_list_hit(const amiga_list_t *l, int16_t y, uint8_t row_h,
                   uint16_t *index)
{
  uint16_t row;
  uint32_t idx;

  if (y < 0 || row_h == 0)
    return 0;
  row = (uint16_t) (y / row_h);
  if (row >= l->rows)
    return 0;
  idx = (uint32_t) l->top + row;
  if (idx >= l->count)
    return 0;
  *index = (uint16_t) idx;
  return 1;
}

void amiga_list_prop(const amiga_list_t *l, uint16_t *pot, uint16_t *body)
{
  uint16_t hidden;

  if (l->count <= l->rows) {
    *pot = 0;
    *body = 0xFFFF;
    return;
  }
  hidden = (uint16_t) (l->count - l->rows);
  *body = (uint16_t) (((uint32_t) l->rows * 0xFFFFUL) / l->count);
  *pot = (uint16_t) (((uint32_t) l->top * 0xFFFFUL) / hidden);
}

void amiga_list_set_top_from_pot(amiga_list_t *l, uint16_t pot)
{
  uint16_t hidden;

  if (l->count <= l->rows) {
    l->top = 0;
    return;
  }
  hidden = (uint16_t) (l->count - l->rows);
  l->top = (uint16_t) (((uint32_t) pot * hidden + 0x7FFFUL) / 0xFFFFUL);
  clamp_top(l);
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test-amiga-host ONLY=list`
Expected: `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add include/platform/amiga/amiga_list.h src/platform/amiga/amiga_list.c tests/amiga makefiles/test-amiga-host.mk
git commit -m "amiga: add listview selection and scroller model

Verified with: make test-amiga-host ONLY=list"
```

---

### Task 3: Size, date and clipping text

**Files:**
- Create: `include/platform/amiga/amiga_format.h`, `src/platform/amiga/amiga_format.c`, `tests/amiga/test_format.c`
- Modify: `tests/amiga/tests.def` (`AMIGA_TEST(format)`), `makefiles/test-amiga-host.mk` (add `amiga_format.c`)

**Interfaces:**
- Produces:
```c
#define AMIGA_SIZE_TEXT_MAX 16
#define AMIGA_DATE_TEXT_MAX 9
void amiga_format_size(char *out, uint32_t size, uint8_t size_format);   /* CONFIG_NIO_PREF_SIZE_* */
int  amiga_format_date(char *out, uint32_t mtime, uint8_t date_format);  /* CONFIG_NIO_PREF_DATE_*; 0 => "??-??-??" */
void amiga_clip_head(char *out, uint16_t cap, const char *s, uint8_t max_chars); /* "long_name..." */
void amiga_clip_tail(char *out, uint16_t cap, const char *s, uint8_t max_chars); /* "...ong/path/" */
```
- The size and date text matches the MS-DOS UI (`dos_format_size_*`, `dos_format_entry_date`) without the column padding. Dates are computed from the Unix time in UTC with a civil-from-days calculation. This avoids depending on `localtime()`, which is unreliable under both Amiga CRTs.

- [ ] **Step 1: Write the failing test** `tests/amiga/test_format.c`

```c
#include "check.h"
#include "config_nio.h"
#include "amiga_format.h"

void test_format(void)
{
  char buf[64];

  amiga_format_size(buf, 0, CONFIG_NIO_PREF_SIZE_FULL);
  CHECK_STR(buf, "0");
  amiga_format_size(buf, 999, CONFIG_NIO_PREF_SIZE_FULL);
  CHECK_STR(buf, "999");
  amiga_format_size(buf, 901120, CONFIG_NIO_PREF_SIZE_FULL);
  CHECK_STR(buf, "901,120");
  amiga_format_size(buf, 4294967295UL, CONFIG_NIO_PREF_SIZE_FULL);
  CHECK_STR(buf, "4,294,967,295");

  amiga_format_size(buf, 999, CONFIG_NIO_PREF_SIZE_COMPACT);
  CHECK_STR(buf, "999");
  amiga_format_size(buf, 1000, CONFIG_NIO_PREF_SIZE_COMPACT);
  CHECK_STR(buf, "1Kb");
  amiga_format_size(buf, 1536, CONFIG_NIO_PREF_SIZE_COMPACT);
  CHECK_STR(buf, "1.5Kb");
  amiga_format_size(buf, 123456, CONFIG_NIO_PREF_SIZE_COMPACT);
  CHECK_STR(buf, "120Kb");
  amiga_format_size(buf, 1048576, CONFIG_NIO_PREF_SIZE_COMPACT);
  CHECK_STR(buf, "1Mb");
  amiga_format_size(buf, 4294967295UL, CONFIG_NIO_PREF_SIZE_COMPACT);
  CHECK_STR(buf, "4Gb");

  CHECK(!amiga_format_date(buf, 0, CONFIG_NIO_PREF_DATE_YMD));
  CHECK_STR(buf, "??-??-??");
  CHECK(amiga_format_date(buf, 951782400UL, CONFIG_NIO_PREF_DATE_YMD));
  CHECK_STR(buf, "00-02-29");
  CHECK(amiga_format_date(buf, 951782400UL, CONFIG_NIO_PREF_DATE_YDM));
  CHECK_STR(buf, "00-29-02");
  CHECK(amiga_format_date(buf, 1709164800UL, CONFIG_NIO_PREF_DATE_YMD));
  CHECK_STR(buf, "24-02-29");
  CHECK(amiga_format_date(buf, 1735689600UL, CONFIG_NIO_PREF_DATE_YMD));
  CHECK_STR(buf, "25-01-01");
  CHECK(amiga_format_date(buf, 4102444799UL, CONFIG_NIO_PREF_DATE_YMD));
  CHECK_STR(buf, "99-12-31");

  amiga_clip_head(buf, sizeof(buf), "short.adf", 20);
  CHECK_STR(buf, "short.adf");
  amiga_clip_head(buf, sizeof(buf), "a_very_long_disk_name.adf", 12);
  CHECK_STR(buf, "a_very_lo...");
  amiga_clip_tail(buf, sizeof(buf), "tnfs://host/games/amiga/demos/", 16);
  CHECK_STR(buf, ".../amiga/demos/");   /* "..." + last 13 characters */
  amiga_clip_head(buf, sizeof(buf), "abcdef", 3);
  CHECK_STR(buf, "abc");
  amiga_clip_head(buf, 4, "abcdefgh", 20);  /* the buffer bounds the output */
  CHECK_STR(buf, "abc");
  amiga_clip_head(buf, sizeof(buf), NULL, 10);
  CHECK_STR(buf, "");
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test-amiga-host ONLY=format`
Expected: compile error, `amiga_format.h: No such file or directory`.

- [ ] **Step 3: Implement** `src/platform/amiga/amiga_format.c`

```c
#include "amiga_format.h"
#include "config_nio.h"

#include <stdio.h>
#include <string.h>

void amiga_format_size(char *out, uint32_t size, uint8_t size_format)
{
  static const char *const suffix[] = { "", "Kb", "Mb", "Gb" };
  char plain[11];
  unsigned unit;
  unsigned long divisor;
  unsigned long rem;
  unsigned whole;
  unsigned tenths;

  if (size_format != CONFIG_NIO_PREF_SIZE_COMPACT) {
    uint8_t len, src, dst, digits;

    sprintf(plain, "%lu", (unsigned long) size);
    len = (uint8_t) strlen(plain);
    dst = (uint8_t) (len + (len - 1) / 3);
    out[dst] = 0;
    src = len;
    digits = 0;
    while (src > 0) {
      if (digits == 3) {
        out[--dst] = ',';
        digits = 0;
      }
      out[--dst] = plain[--src];
      digits++;
    }
    return;
  }

  /* Same rounding as the MS-DOS UI's compact size column. */
  unit = 0;
  divisor = 1UL;
  while (size / divisor >= 1000UL && unit < 3) {
    divisor *= 1024UL;
    unit++;
  }
  if (unit == 0) {
    sprintf(out, "%lu", (unsigned long) size);
    return;
  }
  whole = (unsigned) (size / divisor);
  rem = size % divisor;
  while (rem > 429496729UL && divisor > 1UL) {
    rem = (rem + 5UL) / 10UL;
    divisor = (divisor + 5UL) / 10UL;
  }
  tenths = (unsigned) ((rem * 10UL + (divisor / 2UL)) / divisor);
  if (tenths >= 10) {
    whole++;
    tenths = 0;
  }
  if (whole < 10 && tenths != 0)
    sprintf(out, "%u.%u%s", whole, tenths, suffix[unit]);
  else
    sprintf(out, "%u%s", whole, suffix[unit]);
}

static void civil_from_days(uint32_t days, unsigned *y, unsigned *m,
                            unsigned *d)
{
  unsigned long z, era, doe, yoe, doy, mp;

  z = (unsigned long) days + 719468UL;
  era = z / 146097UL;
  doe = z - era * 146097UL;
  yoe = (doe - doe / 1460UL + doe / 36524UL - doe / 146096UL) / 365UL;
  doy = doe - (365UL * yoe + yoe / 4UL - yoe / 100UL);
  mp = (5UL * doy + 2UL) / 153UL;
  *d = (unsigned) (doy - (153UL * mp + 2UL) / 5UL + 1UL);
  *m = (unsigned) (mp < 10UL ? mp + 3UL : mp - 9UL);
  *y = (unsigned) (yoe + era * 400UL + (*m <= 2 ? 1UL : 0UL));
}

int amiga_format_date(char *out, uint32_t mtime, uint8_t date_format)
{
  unsigned y, m, d;

  if (mtime == 0) {
    strcpy(out, "??-??-??");
    return 0;
  }
  civil_from_days(mtime / 86400UL, &y, &m, &d);
  if (date_format == CONFIG_NIO_PREF_DATE_YDM)
    sprintf(out, "%02u-%02u-%02u", y % 100, d, m);
  else
    sprintf(out, "%02u-%02u-%02u", y % 100, m, d);
  return 1;
}

static void copy_bounded(char *out, uint16_t cap, const char *s, uint16_t n)
{
  if (n >= cap)
    n = (uint16_t) (cap - 1);
  memcpy(out, s, n);
  out[n] = 0;
}

void amiga_clip_head(char *out, uint16_t cap, const char *s, uint8_t max_chars)
{
  uint16_t len;

  if (!out || cap == 0)
    return;
  if (!s)
    s = "";
  len = (uint16_t) strlen(s);
  if (len <= max_chars || max_chars < 4) {
    copy_bounded(out, cap, s, len < max_chars ? len : max_chars);
    return;
  }
  copy_bounded(out, cap, s, (uint16_t) (max_chars - 3));
  if ((uint16_t) (strlen(out) + 3) < cap)
    strcat(out, "...");
}

void amiga_clip_tail(char *out, uint16_t cap, const char *s, uint8_t max_chars)
{
  uint16_t len;

  if (!out || cap == 0)
    return;
  if (!s)
    s = "";
  len = (uint16_t) strlen(s);
  if (len <= max_chars || max_chars < 4) {
    copy_bounded(out, cap, s + (len > max_chars ? len - max_chars : 0),
                 len < max_chars ? len : max_chars);
    return;
  }
  if (cap < 4) {
    out[0] = 0;
    return;
  }
  strcpy(out, "...");
  copy_bounded(out + 3, (uint16_t) (cap - 3), s + len - (max_chars - 3),
               (uint16_t) (max_chars - 3));
}
```

`include/platform/amiga/amiga_format.h`: the declarations in **Interfaces**, with `#include <stdint.h>` and an include guard.

- [ ] **Step 4: Run to verify it passes**

Run: `make test-amiga-host ONLY=format`
Expected: `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add include/platform/amiga/amiga_format.h src/platform/amiga/amiga_format.c tests/amiga makefiles/test-amiga-host.mk
git commit -m "amiga: add size, date and clipping text helpers

Verified with: make test-amiga-host ONLY=format"
```

---

### Task 4: Options (Shell arguments and ToolTypes)

**Files:**
- Create: `include/platform/amiga/amiga_options.h`, `src/platform/amiga/amiga_options.c`, `tests/amiga/test_options.c`
- Modify: `tests/amiga/tests.def` (`AMIGA_TEST(options)`), `makefiles/test-amiga-host.mk`

**Interfaces:**
- Produces:
```c
#define AMIGA_OPT_PATH_MAX 96
#define AMIGA_DEFAULT_RESULT "RAM:config-nio.result"
typedef struct {
  char fmount[AMIGA_OPT_PATH_MAX];
  char fumount[AMIGA_OPT_PATH_MAX];
  char script[AMIGA_OPT_PATH_MAX];
  char result[AMIGA_OPT_PATH_MAX];
} amiga_options_t;
void amiga_options_defaults(amiga_options_t *o);
int  amiga_options_parse(amiga_options_t *o, const char *arg); /* 1 ok, 0 invalid value, -1 unknown/ignored key */
void amiga_options_finish(amiga_options_t *o);                 /* RESULT default when SCRIPT given */
```
- Keys are case-insensitive: `FMOUNT`, `FUMOUNT`, `SCRIPT`, `RESULT`. A ToolType wrapped in parentheses, such as `(FMOUNT=…)`, is disabled by Workbench convention and returns -1 without changing anything.

- [ ] **Step 1: Write the failing test** `tests/amiga/test_options.c`

```c
#include "check.h"
#include "amiga_options.h"

void test_options(void)
{
  amiga_options_t o;
  char longval[120];

  amiga_options_defaults(&o);
  CHECK_STR(o.fmount, "SYS:C/fmount");
  CHECK_STR(o.fumount, "SYS:C/fumount");
  CHECK_STR(o.script, "");
  CHECK_STR(o.result, "");

  CHECK(amiga_options_parse(&o, "fmount=DH0:C/fmount") == 1);
  CHECK_STR(o.fmount, "DH0:C/fmount");
  CHECK(amiga_options_parse(&o, "SCRIPT=NIO:accept.script") == 1);
  CHECK_STR(o.script, "NIO:accept.script");
  CHECK(amiga_options_parse(&o, "DONOTWAIT") == -1);
  CHECK(amiga_options_parse(&o, "(FUMOUNT=X:fumount)") == -1);
  CHECK_STR(o.fumount, "SYS:C/fumount");
  CHECK(amiga_options_parse(&o, "BOGUS=1") == -1);
  CHECK(amiga_options_parse(&o, "FMOUNT=") == 0);
  CHECK_STR(o.fmount, "DH0:C/fmount");

  memset(longval, 'x', sizeof(longval));
  memcpy(longval, "RESULT=", 7);
  longval[sizeof(longval) - 1] = 0;
  CHECK(amiga_options_parse(&o, longval) == 0);
  CHECK_STR(o.result, "");

  amiga_options_finish(&o);
  CHECK_STR(o.result, "RAM:config-nio.result");
  CHECK(amiga_options_parse(&o, "result=T:out") == 1);
  amiga_options_finish(&o);
  CHECK_STR(o.result, "T:out");
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test-amiga-host ONLY=options`
Expected: compile error, missing `amiga_options.h`.

- [ ] **Step 3: Implement** `src/platform/amiga/amiga_options.c`

```c
#include "amiga_options.h"
#include "amiga_drives.h"

#include <string.h>

static int key_is(const char *arg, uint16_t len, const char *key)
{
  uint16_t i;

  if (strlen(key) != len)
    return 0;
  for (i = 0; i < len; i++) {
    char c = arg[i];
    if (c >= 'a' && c <= 'z')
      c = (char) (c - 32);
    if (c != key[i])
      return 0;
  }
  return 1;
}

void amiga_options_defaults(amiga_options_t *o)
{
  memset(o, 0, sizeof(*o));
  strcpy(o->fmount, AMIGA_DEFAULT_FMOUNT);
  strcpy(o->fumount, AMIGA_DEFAULT_FUMOUNT);
}

int amiga_options_parse(amiga_options_t *o, const char *arg)
{
  const char *eq;
  const char *value;
  char *dst;
  uint16_t key_len;

  if (!o || !arg || arg[0] == '(')
    return -1;
  eq = strchr(arg, '=');
  if (!eq)
    return -1;
  key_len = (uint16_t) (eq - arg);
  if (key_is(arg, key_len, "FMOUNT"))
    dst = o->fmount;
  else if (key_is(arg, key_len, "FUMOUNT"))
    dst = o->fumount;
  else if (key_is(arg, key_len, "SCRIPT"))
    dst = o->script;
  else if (key_is(arg, key_len, "RESULT"))
    dst = o->result;
  else
    return -1;
  value = eq + 1;
  if (!value[0] || strlen(value) >= AMIGA_OPT_PATH_MAX)
    return 0;
  strcpy(dst, value);
  return 1;
}

void amiga_options_finish(amiga_options_t *o)
{
  if (o->script[0] && !o->result[0])
    strcpy(o->result, AMIGA_DEFAULT_RESULT);
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test-amiga-host ONLY=options`
Expected: `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add include/platform/amiga/amiga_options.h src/platform/amiga/amiga_options.c tests/amiga makefiles/test-amiga-host.mk
git commit -m "amiga: parse Shell arguments and Workbench ToolTypes

Verified with: make test-amiga-host ONLY=options"
```

---

### Task 5: Look-and-feel primitives (pens and keys)

**Files:**
- Create: `include/platform/amiga/amiga_theme.h`, `src/platform/amiga/amiga_theme.c`, `include/platform/amiga/amiga_input.h`, `src/platform/amiga/amiga_input.c`, `tests/amiga/test_theme.c`, `tests/amiga/test_input.c`
- Modify: `tests/amiga/tests.def` (`AMIGA_TEST(theme)`, `AMIGA_TEST(input)`), `makefiles/test-amiga-host.mk`

**Interfaces:**
- Produces (`amiga_theme.h`). The index constants copy the `intuition/screens.h` values, so pure code does not need Amiga headers:
```c
enum { AMIGA_DETAILPEN = 0, AMIGA_BLOCKPEN, AMIGA_TEXTPEN, AMIGA_SHINEPEN, AMIGA_SHADOWPEN,
       AMIGA_FILLPEN, AMIGA_FILLTEXTPEN, AMIGA_BACKGROUNDPEN, AMIGA_HIGHLIGHTTEXTPEN };
typedef struct { uint8_t text, shine, shadow, fill, filltext, background, highlight; } amiga_theme_t;
void amiga_theme_classic(amiga_theme_t *t);                                     /* KS1.x */
void amiga_theme_from_pens(amiga_theme_t *t, const uint16_t *pens, uint16_t count); /* V36+ DrawInfo */
```
- Produces (`amiga_input.h`):
```c
typedef enum { AMIGA_KEY_NONE = 0, AMIGA_KEY_UP, AMIGA_KEY_DOWN, AMIGA_KEY_PAGE_UP, AMIGA_KEY_PAGE_DOWN,
  AMIGA_KEY_TOP, AMIGA_KEY_BOTTOM, AMIGA_KEY_ACTIVATE, AMIGA_KEY_PARENT, AMIGA_KEY_CANCEL,
  AMIGA_KEY_HELP, AMIGA_KEY_NEXT_PAGE, AMIGA_KEY_PREV_PAGE } amiga_key_t;
amiga_key_t amiga_key_from_raw(uint16_t code, uint16_t qualifier);
```
RAWKEY codes: up 0x4C, down 0x4D, Return 0x44, keypad Enter 0x43, Esc 0x45, Help 0x5F, Backspace 0x41, Tab 0x42. Key-up events (bit 0x80) map to NONE. Qualifiers: Shift 0x0003, Alt 0x0030.

- [ ] **Step 1: Write the failing tests**

`tests/amiga/test_theme.c`:
```c
#include "check.h"
#include "amiga_theme.h"

void test_theme(void)
{
  amiga_theme_t t;
  /* Default V36 4-colour pens: detail block text shine shadow fill filltext background highlight */
  static const uint16_t v36[9] = { 0, 1, 1, 2, 1, 3, 1, 0, 2 };
  static const uint16_t custom[9] = { 0, 1, 5, 6, 7, 4, 2, 3, 6 };

  amiga_theme_classic(&t);
  CHECK(t.background == 0 && t.text == 1 && t.shine == 1 && t.shadow == 2);
  CHECK(t.fill == 3 && t.filltext == 2 && t.highlight == 3);

  amiga_theme_from_pens(&t, v36, 9);
  CHECK(t.text == 1 && t.shine == 2 && t.shadow == 1 && t.fill == 3);
  CHECK(t.filltext == 1 && t.background == 0 && t.highlight == 2);

  amiga_theme_from_pens(&t, custom, 9);
  CHECK(t.text == 5 && t.shine == 6 && t.shadow == 7 && t.fill == 4);
  CHECK(t.filltext == 2 && t.background == 3 && t.highlight == 6);

  /* A short pen array keeps the V36 defaults for missing roles. */
  amiga_theme_from_pens(&t, custom, 4);
  CHECK(t.text == 5 && t.shine == 6 && t.shadow == 1 && t.fill == 3);

  amiga_theme_from_pens(&t, NULL, 0);
  CHECK(t.text == 1 && t.shine == 2);
}
```

`tests/amiga/test_input.c`:
```c
#include "check.h"
#include "amiga_input.h"

void test_input(void)
{
  CHECK(amiga_key_from_raw(0x4C, 0) == AMIGA_KEY_UP);
  CHECK(amiga_key_from_raw(0x4D, 0) == AMIGA_KEY_DOWN);
  CHECK(amiga_key_from_raw(0x4C, 0x0001) == AMIGA_KEY_PAGE_UP);
  CHECK(amiga_key_from_raw(0x4D, 0x0002) == AMIGA_KEY_PAGE_DOWN);
  CHECK(amiga_key_from_raw(0x4C, 0x0010) == AMIGA_KEY_TOP);
  CHECK(amiga_key_from_raw(0x4D, 0x0020) == AMIGA_KEY_BOTTOM);
  CHECK(amiga_key_from_raw(0x44, 0) == AMIGA_KEY_ACTIVATE);
  CHECK(amiga_key_from_raw(0x43, 0) == AMIGA_KEY_ACTIVATE);
  CHECK(amiga_key_from_raw(0x41, 0) == AMIGA_KEY_PARENT);
  CHECK(amiga_key_from_raw(0x45, 0) == AMIGA_KEY_CANCEL);
  CHECK(amiga_key_from_raw(0x5F, 0) == AMIGA_KEY_HELP);
  CHECK(amiga_key_from_raw(0x42, 0) == AMIGA_KEY_NEXT_PAGE);
  CHECK(amiga_key_from_raw(0x42, 0x0001) == AMIGA_KEY_PREV_PAGE);
  CHECK(amiga_key_from_raw(0x4C | 0x80, 0) == AMIGA_KEY_NONE);
  CHECK(amiga_key_from_raw(0x20, 0) == AMIGA_KEY_NONE);
}
```

- [ ] **Step 2: Run to verify they fail**

Run: `make test-amiga-host ONLY=theme; make test-amiga-host ONLY=input`
Expected: compile errors for the missing headers.

- [ ] **Step 3: Implement**

`src/platform/amiga/amiga_theme.c`:
```c
#include "amiga_theme.h"

/* Workbench 1.x palette: 0 blue, 1 white, 2 black, 3 orange. */
void amiga_theme_classic(amiga_theme_t *t)
{
  t->background = 0;
  t->text = 1;
  t->shine = 1;
  t->shadow = 2;
  t->fill = 3;
  t->filltext = 2;
  t->highlight = 3;
}

static uint8_t pen(const uint16_t *pens, uint16_t count, uint16_t index,
                   uint8_t fallback)
{
  return (uint8_t) (pens && index < count ? pens[index] : fallback);
}

void amiga_theme_from_pens(amiga_theme_t *t, const uint16_t *pens,
                           uint16_t count)
{
  t->text = pen(pens, count, AMIGA_TEXTPEN, 1);
  t->shine = pen(pens, count, AMIGA_SHINEPEN, 2);
  t->shadow = pen(pens, count, AMIGA_SHADOWPEN, 1);
  t->fill = pen(pens, count, AMIGA_FILLPEN, 3);
  t->filltext = pen(pens, count, AMIGA_FILLTEXTPEN, 1);
  t->background = pen(pens, count, AMIGA_BACKGROUNDPEN, 0);
  t->highlight = pen(pens, count, AMIGA_HIGHLIGHTTEXTPEN, 2);
}
```

`src/platform/amiga/amiga_input.c`:
```c
#include "amiga_input.h"

#define RAW_UP 0x4C
#define RAW_DOWN 0x4D
#define RAW_RETURN 0x44
#define RAW_ENTER 0x43
#define RAW_ESC 0x45
#define RAW_HELP 0x5F
#define RAW_BACKSPACE 0x41
#define RAW_TAB 0x42
#define QUAL_SHIFT 0x0003
#define QUAL_ALT 0x0030

amiga_key_t amiga_key_from_raw(uint16_t code, uint16_t qualifier)
{
  if (code & 0x80)
    return AMIGA_KEY_NONE;
  switch (code) {
  case RAW_UP:
    if (qualifier & QUAL_ALT) return AMIGA_KEY_TOP;
    return (qualifier & QUAL_SHIFT) ? AMIGA_KEY_PAGE_UP : AMIGA_KEY_UP;
  case RAW_DOWN:
    if (qualifier & QUAL_ALT) return AMIGA_KEY_BOTTOM;
    return (qualifier & QUAL_SHIFT) ? AMIGA_KEY_PAGE_DOWN : AMIGA_KEY_DOWN;
  case RAW_RETURN:
  case RAW_ENTER:
    return AMIGA_KEY_ACTIVATE;
  case RAW_BACKSPACE:
    return AMIGA_KEY_PARENT;
  case RAW_ESC:
    return AMIGA_KEY_CANCEL;
  case RAW_HELP:
    return AMIGA_KEY_HELP;
  case RAW_TAB:
    return (qualifier & QUAL_SHIFT) ? AMIGA_KEY_PREV_PAGE : AMIGA_KEY_NEXT_PAGE;
  default:
    return AMIGA_KEY_NONE;
  }
}
```

Headers: the declarations in **Interfaces**, with `#include <stdint.h>` and include guards.

- [ ] **Step 4: Run to verify they pass**

Run: `make test-amiga-host`
Expected: all tests so far pass, `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add include/platform/amiga/amiga_theme.h include/platform/amiga/amiga_input.h src/platform/amiga/amiga_theme.c src/platform/amiga/amiga_input.c tests/amiga makefiles/test-amiga-host.mk
git commit -m "amiga: add Workbench pen roles and keyboard mapping

Verified with: make test-amiga-host"
```

---

### Task 6: Window layout

**Files:**
- Create: `include/platform/amiga/amiga_layout.h`, `src/platform/amiga/amiga_layout.c`, `tests/amiga/test_layout.c`
- Modify: `tests/amiga/tests.def` (`AMIGA_TEST(layout)`), `makefiles/test-amiga-host.mk`

**Interfaces:**
- Produces:
```c
#define AMIGA_LAYOUT_PAD 4
#define AMIGA_LAYOUT_GAP 3
#define AMIGA_LAYOUT_COLS 76
#define AMIGA_LAYOUT_MIN_ROWS 6
#define AMIGA_LAYOUT_MAX_ROWS 16
#define AMIGA_SCROLLER_W 16
#define AMIGA_TAB_COUNT 4
#define AMIGA_BUTTON_COUNT 6
typedef struct { int16_t left, top, width, height; } amiga_rect_t;
typedef struct {
  uint16_t screen_w, screen_h;
  uint8_t border_l, border_t, border_r, border_b;   /* the opened window's real borders */
  uint8_t font_w, font_h;                          /* rendering font (topaz 8 = 8, 8) */
  uint8_t gadget_font_h;                           /* font string gadgets draw with (screen font) */
} amiga_layout_in_t;
typedef struct {
  uint16_t win_w, win_h;
  uint8_t row_h, list_rows;
  amiga_rect_t tab[AMIGA_TAB_COUNT];
  amiga_rect_t info;       /* text line above the list */
  amiga_rect_t list;       /* list frame (bevel is the outer 2px) */
  amiga_rect_t scroller;
  amiga_rect_t edit;       /* frames: GUI insets gadgets by (4,3) */
  amiga_rect_t slot;
  amiga_rect_t ro;
  amiga_rect_t button[AMIGA_BUTTON_COUNT];
  amiga_rect_t status;
} amiga_layout_t;
int amiga_layout_compute(const amiga_layout_in_t *in, amiga_layout_t *out); /* 0 = screen too small */
amiga_rect_t amiga_rect_inset(amiga_rect_t r, int16_t dx, int16_t dy);
```
- Algorithm. Here `cw = COLS*font_w`, `btn_h = font_h+6`, `str_h = max(font_h, gadget_font_h)+6`, `row_h = font_h+1`, and `x0 = border_l + PAD`.
  - Window width is `border_l + PAD + cw + PAD + border_r`.
  - Rows are placed top to bottom: tabs, info line (`font_h+2`), list frame (`rows*row_h+4`, with the scroller on the right and `SCROLLER_W` wide), edit row (`str_h`), button row (`btn_h`), status line (`font_h+4`), each separated by GAP, then PAD and `border_b`.
  - `rows = (screen_h − list_top − fixed_below − 4) / row_h`, clamped to at most MAX_ROWS. If rows is below MIN_ROWS, the function returns 0.
  - Edit row: label space of `4*font_w+4` for "URI", then `edit`, a GAP, label space `4*font_w+4` for "Slot", then `slot` (`3*font_w+12` wide), a GAP, `ro` (18 wide) plus label space `2*font_w+4`. `edit` takes the remaining width.
  - Tabs and buttons are equal widths separated by GAP. The last one takes the remainder.

- [ ] **Step 1: Write the failing test** `tests/amiga/test_layout.c`

```c
#include "check.h"
#include "amiga_layout.h"

static int inside(const amiga_layout_in_t *in, const amiga_layout_t *o,
                  amiga_rect_t r)
{
  return r.left >= in->border_l && r.top >= in->border_t &&
         r.width > 0 && r.height > 0 &&
         r.left + r.width <= o->win_w - in->border_r &&
         r.top + r.height <= o->win_h - in->border_b;
}

static void check_geometry(const amiga_layout_in_t *in, const amiga_layout_t *o)
{
  int i;

  CHECK(o->win_w <= in->screen_w && o->win_h <= in->screen_h);
  for (i = 0; i < AMIGA_TAB_COUNT; i++)
    CHECK(inside(in, o, o->tab[i]));
  for (i = 0; i < AMIGA_BUTTON_COUNT; i++)
    CHECK(inside(in, o, o->button[i]));
  CHECK(inside(in, o, o->info) && inside(in, o, o->list) &&
        inside(in, o, o->scroller) && inside(in, o, o->edit) &&
        inside(in, o, o->slot) && inside(in, o, o->ro) &&
        inside(in, o, o->status));
  /* rows are stacked, never overlapping */
  CHECK(o->info.top >= o->tab[0].top + o->tab[0].height);
  CHECK(o->list.top >= o->info.top + o->info.height);
  CHECK(o->edit.top >= o->list.top + o->list.height);
  CHECK(o->button[0].top >= o->edit.top + o->edit.height);
  CHECK(o->status.top >= o->button[0].top + o->button[0].height);
  /* edit row pieces left to right, with room for their labels */
  CHECK(o->edit.left + o->edit.width + 4 * in->font_w + 4 <= o->slot.left);
  CHECK(o->slot.left + o->slot.width <= o->ro.left);
  CHECK(o->scroller.left == o->list.left + o->list.width);
  CHECK(o->list.height == o->list_rows * o->row_h + 4);
  CHECK(o->tab[0].width == o->tab[1].width);
  CHECK(o->button[0].width == o->button[4].width);
}

void test_layout(void)
{
  amiga_layout_t o;
  /* KS1.3 NTSC Workbench, topaz 8 everywhere. */
  amiga_layout_in_t ntsc13 = { 640, 200, 4, 11, 4, 2, 8, 8, 8 };
  /* WB3.2 PAL. */
  amiga_layout_in_t pal32 = { 640, 256, 4, 11, 4, 2, 8, 8, 8 };
  /* WB3.x with a 16px title font and a 13px screen font. */
  amiga_layout_in_t tall = { 640, 200, 4, 19, 4, 2, 8, 8, 13 };
  amiga_layout_in_t narrow = { 320, 200, 4, 11, 4, 2, 8, 8, 8 };
  amiga_layout_in_t shallow = { 640, 150, 4, 11, 4, 2, 8, 8, 8 };

  CHECK(amiga_layout_compute(&ntsc13, &o));
  CHECK(o.win_w == 624 && o.win_h == 194 && o.list_rows == 10 && o.row_h == 9);
  check_geometry(&ntsc13, &o);

  CHECK(amiga_layout_compute(&pal32, &o));
  CHECK(o.list_rows == 16 && o.win_h == 248);
  check_geometry(&pal32, &o);

  CHECK(amiga_layout_compute(&tall, &o));
  CHECK(o.list_rows >= AMIGA_LAYOUT_MIN_ROWS && o.edit.height == 19);
  check_geometry(&tall, &o);

  CHECK(!amiga_layout_compute(&narrow, &o));
  CHECK(!amiga_layout_compute(&shallow, &o));
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test-amiga-host ONLY=layout`
Expected: compile error, missing `amiga_layout.h`.

- [ ] **Step 3: Implement** `src/platform/amiga/amiga_layout.c`

```c
#include "amiga_layout.h"

static amiga_rect_t rect(int16_t left, int16_t top, int16_t width,
                         int16_t height)
{
  amiga_rect_t r;
  r.left = left;
  r.top = top;
  r.width = width;
  r.height = height;
  return r;
}

amiga_rect_t amiga_rect_inset(amiga_rect_t r, int16_t dx, int16_t dy)
{
  return rect((int16_t) (r.left + dx), (int16_t) (r.top + dy),
              (int16_t) (r.width - 2 * dx), (int16_t) (r.height - 2 * dy));
}

static void spread(amiga_rect_t *out, uint8_t n, int16_t x0, int16_t y,
                   int16_t total_w, int16_t h)
{
  int16_t w;
  uint8_t i;

  w = (int16_t) ((total_w - AMIGA_LAYOUT_GAP * (n - 1)) / n);
  for (i = 0; i < n; i++) {
    int16_t left = (int16_t) (x0 + i * (w + AMIGA_LAYOUT_GAP));
    out[i] = rect(left, y, i + 1 == n ? (int16_t) (x0 + total_w - left) : w, h);
  }
}

int amiga_layout_compute(const amiga_layout_in_t *in, amiga_layout_t *out)
{
  int16_t cw, x0, y, btn_h, str_h, info_h, status_h, list_top, fixed_below;
  int16_t label_w, slot_w, ro_w, ro_label_w, avail, rows, right;
  uint8_t gfh;

  cw = (int16_t) (AMIGA_LAYOUT_COLS * in->font_w);
  x0 = (int16_t) (in->border_l + AMIGA_LAYOUT_PAD);
  out->win_w = (uint16_t) (in->border_l + AMIGA_LAYOUT_PAD + cw +
                           AMIGA_LAYOUT_PAD + in->border_r);
  if (out->win_w > in->screen_w)
    return 0;

  gfh = in->gadget_font_h > in->font_h ? in->gadget_font_h : in->font_h;
  btn_h = (int16_t) (in->font_h + 6);
  str_h = (int16_t) (gfh + 6);
  info_h = (int16_t) (in->font_h + 2);
  status_h = (int16_t) (in->font_h + 4);
  out->row_h = (uint8_t) (in->font_h + 1);

  y = (int16_t) (in->border_t + AMIGA_LAYOUT_PAD);
  spread(out->tab, AMIGA_TAB_COUNT, x0, y, cw, btn_h);
  y = (int16_t) (y + btn_h + AMIGA_LAYOUT_GAP);
  out->info = rect(x0, y, cw, info_h);
  y = (int16_t) (y + info_h + AMIGA_LAYOUT_GAP);

  list_top = y;
  fixed_below = (int16_t) (AMIGA_LAYOUT_GAP + str_h + AMIGA_LAYOUT_GAP +
                           btn_h + AMIGA_LAYOUT_GAP + status_h +
                           AMIGA_LAYOUT_PAD + in->border_b);
  avail = (int16_t) (in->screen_h - list_top - fixed_below - 4);
  rows = (int16_t) (avail / out->row_h);
  if (rows > AMIGA_LAYOUT_MAX_ROWS)
    rows = AMIGA_LAYOUT_MAX_ROWS;
  if (rows < AMIGA_LAYOUT_MIN_ROWS)
    return 0;
  out->list_rows = (uint8_t) rows;
  out->list = rect(x0, list_top, (int16_t) (cw - AMIGA_SCROLLER_W),
                   (int16_t) (rows * out->row_h + 4));
  out->scroller = rect((int16_t) (x0 + cw - AMIGA_SCROLLER_W), list_top,
                       AMIGA_SCROLLER_W, out->list.height);
  y = (int16_t) (list_top + out->list.height + AMIGA_LAYOUT_GAP);

  label_w = (int16_t) (4 * in->font_w + 4);
  slot_w = (int16_t) (3 * in->font_w + 12);
  ro_w = 18;
  ro_label_w = (int16_t) (2 * in->font_w + 4);
  right = (int16_t) (x0 + cw);
  out->ro = rect((int16_t) (right - ro_label_w - ro_w), y, ro_w, str_h);
  out->slot = rect((int16_t) (out->ro.left - AMIGA_LAYOUT_GAP - slot_w), y,
                   slot_w, str_h);
  out->edit = rect((int16_t) (x0 + label_w), y,
                   (int16_t) (out->slot.left - label_w - AMIGA_LAYOUT_GAP -
                              (x0 + label_w)),
                   str_h);
  y = (int16_t) (y + str_h + AMIGA_LAYOUT_GAP);

  spread(out->button, AMIGA_BUTTON_COUNT, x0, y, cw, btn_h);
  y = (int16_t) (y + btn_h + AMIGA_LAYOUT_GAP);
  out->status = rect(x0, y, cw, status_h);
  y = (int16_t) (y + status_h + AMIGA_LAYOUT_PAD);
  out->win_h = (uint16_t) (y + in->border_b);
  return 1;
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test-amiga-host ONLY=layout`
Expected: `0 failures`. If an exact-value check (`win_h == 194`, `248`) is off by the arithmetic, recompute by hand from the algorithm above. The algorithm is the contract, so fix whichever side disagrees with it and state which one in the commit message.

- [ ] **Step 5: Commit**

```bash
git add include/platform/amiga/amiga_layout.h src/platform/amiga/amiga_layout.c tests/amiga makefiles/test-amiga-host.mk
git commit -m "amiga: compute window layout from real borders and fonts

Verified with: make test-amiga-host ONLY=layout"
```

---

### Task 7: Fake NIO and controller (hosts and preferences)

**Files:**
- Create: `tests/amiga/fake_nio.h`, `tests/amiga/fake_nio.c`, `tests/amiga/test_ctl_hosts.c`
- Create: `include/platform/amiga/amiga_ctl.h`, `src/platform/amiga/amiga_ctl.c`
- Modify: `tests/amiga/tests.def` (`AMIGA_TEST(ctl_hosts)`), `makefiles/test-amiga-host.mk`:
  - Add `src/platform/amiga/amiga_ctl.c` to `AMIGA_HOST_SRCS`.
  - Set `PORTABLE_HOST_SRCS := src/platform/portable/config_nio_state.c src/platform/portable/config_nio_store.c src/platform/portable/config_nio_tables.c`.
  - Add `tests/amiga/fake_nio.c` to `TEST_SUPPORT_SRCS`.

**Interfaces:**
- Consumes: `amiga_list_*` (Task 2), `amiga_drives.h` constants (Task 1), and from `config_nio.h`: `config_nio_load`, `config_nio_save_hosts`, `config_nio_save_prefs`, `config_nio_host_set`, `config_nio_set_status`.
- Produces (`amiga_ctl.h`; later tasks add functions to it):
```c
#include "config_nio.h"
#include "amiga_list.h"
typedef enum { AMIGA_PAGE_HOSTS = 0, AMIGA_PAGE_BROWSE, AMIGA_PAGE_CATALOGUE, AMIGA_PAGE_DRIVES, AMIGA_PAGE_COUNT } amiga_page_t;
#define AMIGA_CAT_WINDOW 16
#define AMIGA_CAT_SLOTS 256
#define AMIGA_CMD_MAX 128
typedef int (*amiga_exec_fn)(const char *command, void *ctx);   /* returns the command's rc */
typedef struct {
  config_nio_state_t *state;
  uint8_t page;
  amiga_list_t hosts, entries, catalogue, drives;
  uint8_t browse_host;
  uint8_t browse_open;
  uint8_t kick13;
  const char *fmount;
  const char *fumount;
  amiga_exec_fn exec;
  void *exec_ctx;
  uint16_t cat_base;                      /* AMIGA_CAT_SLOTS = cache empty */
  config_nio_slot_t cat[AMIGA_CAT_WINDOW];
  char msg[CONFIG_NIO_STATUS_MAX + 1];
  char cmd[AMIGA_CMD_MAX];
} amiga_ctl_t;
void amiga_ctl_init(amiga_ctl_t *ctl, config_nio_state_t *state, uint8_t kick13, uint8_t rows,
                    amiga_exec_fn exec, void *exec_ctx);
void amiga_ctl_set_tools(amiga_ctl_t *ctl, const char *fmount, const char *fumount);
void amiga_ctl_set_rows(amiga_ctl_t *ctl, uint8_t rows);
void amiga_ctl_set_page(amiga_ctl_t *ctl, uint8_t page);
int  amiga_ctl_host_add(amiga_ctl_t *ctl, const char *uri);
int  amiga_ctl_host_replace(amiga_ctl_t *ctl, const char *uri);
int  amiga_ctl_host_remove(amiga_ctl_t *ctl);
int  amiga_ctl_host_move(amiga_ctl_t *ctl, int8_t delta);
int  amiga_ctl_set_prefs(amiga_ctl_t *ctl, uint8_t date_format, uint8_t size_format);
```
- Produces (`fake_nio.h`, test-only):
```c
typedef struct { uint8_t is_dir; const char *name; uint32_t size; uint32_t mtime; } fake_dir_entry_t;
void fake_nio_reset(void);
void fake_appstore_put(const char *ns, const char *key, const void *data, uint16_t len);
const uint8_t *fake_appstore_get(const char *ns, const char *key, uint16_t *len); /* NULL if absent */
void fake_slot_put(uint8_t index, const char *uri, uint8_t readonly);
const char *fake_slot_uri(uint8_t index);   /* NULL when empty */
uint8_t fake_slot_readonly(uint8_t index);
unsigned fake_slot_get_calls(void);
void fake_dir_put(const char *uri, const fake_dir_entry_t *entries, uint8_t count);
void fake_dir_fail(const char *uri);
```
The fake implements every fujinet-nio and fnsvc/fnctl symbol that the linked portable files and the controller reference:
- `fn_appstore_read`, `fn_appstore_write`, `fn_appstore_delete`
- `fn_slot_catalog_get`, `fn_slot_catalog_put`, `fn_slot_catalog_delete`
- `fnsvc_list_directory`, `fnsvc_last_error`, `fnsvc_last_status`, `fnsvc_disk_mount`
- `fnctl_set_unit_slot`

- [ ] **Step 1: Write the fake** `tests/amiga/fake_nio.c`

```c
/* In-memory stand-in for the fujinet-nio calls used by config_nio_store.c,
 * config_nio_state.c and amiga_ctl.c.  Behaviour follows the documented
 * contracts in fujinet-nio.h (missing keys: FN_OK, EXISTS clear, EOF set). */
#include "fake_nio.h"
#include "fnctl.h"
#include "fnsvc.h"
#include "fujinet-nio.h"

#include <string.h>

#define FAKE_KEYS 16
#define FAKE_VALUE_MAX 4096
#define FAKE_DIRS 8

typedef struct {
  uint8_t used;
  char ns[24];
  char key[24];
  uint16_t len;
  uint8_t data[FAKE_VALUE_MAX];
} fake_key_t;

typedef struct {
  uint8_t used;
  uint8_t readonly;
  char uri[FNSVC_MAX_URI + 1];
} fake_slot_t;

typedef struct {
  uint8_t used;
  uint8_t fail;
  char uri[FNSVC_MAX_URI + 1];
  const fake_dir_entry_t *entries;
  uint8_t count;
} fake_dir_t;

static fake_key_t keys[FAKE_KEYS];
static fake_slot_t slots[256];
static fake_dir_t dirs[FAKE_DIRS];
static unsigned slot_get_calls;
static uint8_t last_error;
static uint8_t last_status;

void fake_nio_reset(void)
{
  memset(keys, 0, sizeof(keys));
  memset(slots, 0, sizeof(slots));
  memset(dirs, 0, sizeof(dirs));
  slot_get_calls = 0;
  last_error = 0;
  last_status = 0;
}

static fake_key_t *find_key(const char *ns, const char *key, int create)
{
  int i;
  fake_key_t *free_key = NULL;

  for (i = 0; i < FAKE_KEYS; i++) {
    if (keys[i].used && !strcmp(keys[i].ns, ns) && !strcmp(keys[i].key, key))
      return &keys[i];
    if (!keys[i].used && !free_key)
      free_key = &keys[i];
  }
  if (!create || !free_key)
    return NULL;
  memset(free_key, 0, sizeof(*free_key));
  free_key->used = 1;
  strcpy(free_key->ns, ns);
  strcpy(free_key->key, key);
  return free_key;
}

void fake_appstore_put(const char *ns, const char *key, const void *data,
                       uint16_t len)
{
  fake_key_t *k = find_key(ns, key, 1);
  memcpy(k->data, data, len);
  k->len = len;
}

const uint8_t *fake_appstore_get(const char *ns, const char *key, uint16_t *len)
{
  fake_key_t *k = find_key(ns, key, 0);
  if (!k)
    return NULL;
  *len = k->len;
  return k->data;
}

uint8_t fn_appstore_read(fn_appstore_io_t *io, const char *ns, const char *key,
                         uint32_t offset, uint8_t *buf, uint16_t max_len,
                         fn_appstore_read_t *out)
{
  fake_key_t *k;
  uint16_t remaining;

  (void) io;
  memset(out, 0, sizeof(*out));
  out->offset = offset;
  k = find_key(ns, key, 0);
  if (!k) {
    out->flags = FN_APPSTORE_READ_EOF;
    return FN_OK;
  }
  out->flags = FN_APPSTORE_READ_EXISTS;
  remaining = offset >= k->len ? 0 : (uint16_t) (k->len - offset);
  if (remaining > max_len) {
    remaining = max_len;
  } else {
    out->flags |= FN_APPSTORE_READ_EOF;
  }
  memcpy(buf, k->data + offset, remaining);
  out->bytes_read = remaining;
  return FN_OK;
}

uint8_t fn_appstore_write(fn_appstore_io_t *io, const char *ns, const char *key,
                          uint32_t offset, const uint8_t *data, uint16_t len,
                          fn_appstore_write_t *out)
{
  fake_key_t *k;

  (void) io;
  k = find_key(ns, key, 1);
  if (!k || offset + len > FAKE_VALUE_MAX)
    return FN_ERR_IO;
  if (offset == 0)
    k->len = 0;
  memcpy(k->data + offset, data, len);
  if (offset + len > k->len)
    k->len = (uint16_t) (offset + len);
  out->offset = offset;
  out->bytes_written = len;
  return FN_OK;
}

uint8_t fn_appstore_delete(fn_appstore_io_t *io, const char *ns,
                           const char *key, fn_appstore_delete_t *out)
{
  fake_key_t *k;

  (void) io;
  k = find_key(ns, key, 0);
  out->deleted = k != NULL;
  if (k)
    k->used = 0;
  return FN_OK;
}

void fake_slot_put(uint8_t index, const char *uri, uint8_t readonly)
{
  slots[index].used = 1;
  slots[index].readonly = readonly;
  strcpy(slots[index].uri, uri);
}

const char *fake_slot_uri(uint8_t index)
{
  return slots[index].used ? slots[index].uri : NULL;
}

uint8_t fake_slot_readonly(uint8_t index)
{
  return slots[index].readonly;
}

unsigned fake_slot_get_calls(void)
{
  return slot_get_calls;
}

static void fill_entry(uint8_t index, fn_slot_catalog_entry_t *out)
{
  out->index = index;
  out->flags = (uint8_t) (FN_SLOT_CATALOG_ENTRY_VALID |
                          (slots[index].readonly ? FN_SLOT_CATALOG_ENTRY_READ_ONLY : 0));
  out->uri_len = (uint16_t) strlen(slots[index].uri);
  out->uri = (const uint8_t *) slots[index].uri;
}

uint8_t fn_slot_catalog_get(fn_slot_catalog_io_t *io, uint8_t index,
                            fn_slot_catalog_entry_t *out)
{
  (void) io;
  slot_get_calls++;
  if (!slots[index].used)
    return FN_ERR_NOT_FOUND;
  fill_entry(index, out);
  return FN_OK;
}

uint8_t fn_slot_catalog_put(fn_slot_catalog_io_t *io, uint8_t index,
                            uint8_t flags, const char *target,
                            fn_slot_catalog_entry_t *out)
{
  (void) io;
  fake_slot_put(index, target,
                (uint8_t) ((flags & FN_SLOT_CATALOG_ENTRY_READ_ONLY) != 0));
  fill_entry(index, out);
  return FN_OK;
}

uint8_t fn_slot_catalog_delete(fn_slot_catalog_io_t *io, uint8_t index,
                               uint8_t *deleted)
{
  (void) io;
  *deleted = slots[index].used;
  memset(&slots[index], 0, sizeof(slots[index]));
  return FN_OK;
}

void fake_dir_put(const char *uri, const fake_dir_entry_t *entries,
                  uint8_t count)
{
  int i;
  for (i = 0; i < FAKE_DIRS; i++) {
    if (!dirs[i].used || !strcmp(dirs[i].uri, uri)) {
      dirs[i].used = 1;
      dirs[i].fail = 0;
      strcpy(dirs[i].uri, uri);
      dirs[i].entries = entries;
      dirs[i].count = count;
      return;
    }
  }
}

void fake_dir_fail(const char *uri)
{
  fake_dir_put(uri, NULL, 0);
  {
    int i;
    for (i = 0; i < FAKE_DIRS; i++)
      if (dirs[i].used && !strcmp(dirs[i].uri, uri))
        dirs[i].fail = 1;
  }
}

int fnsvc_list_directory(const char *uri, fnsvc_list_cb cb, void *ctx)
{
  int i;
  uint8_t e;

  for (i = 0; i < FAKE_DIRS; i++) {
    if (dirs[i].used && !strcmp(dirs[i].uri, uri) && !dirs[i].fail) {
      for (e = 0; e < dirs[i].count; e++)
        cb(dirs[i].entries[e].is_dir, dirs[i].entries[e].name,
           dirs[i].entries[e].size, dirs[i].entries[e].mtime, ctx);
      last_error = FNSVC_ERR_NONE;
      last_status = 0;
      return 1;
    }
  }
  last_error = FNSVC_ERR_STATUS;
  last_status = 5;
  return 0;
}

uint8_t fnsvc_last_error(void) { return last_error; }
uint8_t fnsvc_last_status(void) { return last_status; }
int fnsvc_disk_mount(uint8_t slot, const char *uri, uint8_t readonly)
{
  (void) slot; (void) uri; (void) readonly;
  return 1;
}
int fnctl_set_unit_slot(uint8_t unit, uint8_t slot)
{
  (void) unit; (void) slot;
  return 1;
}
```

`tests/amiga/fake_nio.h`: the declarations in **Interfaces**, with `#include <stdint.h>`.

- [ ] **Step 2: Write the failing test** `tests/amiga/test_ctl_hosts.c`

```c
#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;

static int no_exec(const char *command, void *ctx)
{
  (void) command;
  (void) ctx;
  return 20;
}

void test_ctl_hosts(void)
{
  static const char seeded[] =
    "sd0:/\nfujinet.diller.org\nfujinet.online\ntnfs://example.org\n";
  const uint8_t *v;
  uint16_t len;

  fake_nio_reset();
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, no_exec, NULL);

  /* First load seeds the same three hosts as every other target. */
  CHECK(state.host_count == 3);
  CHECK(ctl.hosts.count == 3 && ctl.hosts.selected == 0);
  CHECK(ctl.catalogue.count == 256 && ctl.drives.count == 8);

  CHECK(amiga_ctl_host_add(&ctl, "tnfs://example.org"));
  CHECK(state.host_count == 4 && ctl.hosts.selected == 3);
  CHECK_STR(state.status, "Host added");
  v = fake_appstore_get("config-nio", "hosts", &len);
  CHECK(v && len == strlen(seeded) && !memcmp(v, seeded, len));

  CHECK(!amiga_ctl_host_add(&ctl, ""));
  CHECK_STR(state.status, "Host is empty");

  amiga_list_select(&ctl.hosts, 1);
  CHECK(amiga_ctl_host_move(&ctl, -1));
  CHECK_STR(state.hosts[0], "fujinet.diller.org");
  CHECK(ctl.hosts.selected == 0);
  CHECK(!amiga_ctl_host_move(&ctl, -1));

  CHECK(amiga_ctl_host_replace(&ctl, "tnfs://replaced"));
  CHECK_STR(state.hosts[0], "tnfs://replaced");
  CHECK(!amiga_ctl_host_replace(&ctl, ""));

  amiga_list_select(&ctl.hosts, 3);
  CHECK(amiga_ctl_host_remove(&ctl));
  CHECK(state.host_count == 3 && ctl.hosts.selected == 2);

  while (state.host_count < CONFIG_NIO_MAX_HOSTS)
    CHECK(amiga_ctl_host_add(&ctl, "tnfs://h"));
  CHECK(!amiga_ctl_host_add(&ctl, "tnfs://one-too-many"));
  CHECK_STR(state.status, "Host list is full");

  while (state.host_count)
    CHECK(amiga_ctl_host_remove(&ctl));
  CHECK(ctl.hosts.selected == AMIGA_LIST_NONE);
  CHECK(!amiga_ctl_host_remove(&ctl));
  CHECK_STR(state.status, "No host selected");
  CHECK(!amiga_ctl_host_move(&ctl, 1));

  CHECK(amiga_ctl_set_prefs(&ctl, CONFIG_NIO_PREF_DATE_YDM,
                            CONFIG_NIO_PREF_SIZE_COMPACT));
  v = fake_appstore_get("config-nio", "prefs", &len);
  CHECK(v && len >= 22 && !memcmp(v, "date=ydm\nsize=compact\n", 22));
  CHECK(state.prefs.date_format == CONFIG_NIO_PREF_DATE_YDM);

  amiga_ctl_set_page(&ctl, AMIGA_PAGE_DRIVES);
  CHECK(ctl.page == AMIGA_PAGE_DRIVES);
  amiga_ctl_set_page(&ctl, 9);
  CHECK(ctl.page == AMIGA_PAGE_DRIVES);
}
```

- [ ] **Step 3: Run to verify it fails**

Run: `make test-amiga-host ONLY=ctl_hosts`
Expected: compile error, missing `amiga_ctl.h`.

- [ ] **Step 4: Implement** `src/platform/amiga/amiga_ctl.c` (hosts, preferences, init)

```c
#include "amiga_ctl.h"
#include "amiga_drives.h"

#include <stdio.h>
#include <string.h>

static void status(amiga_ctl_t *ctl, const char *msg)
{
  config_nio_set_status(ctl->state, msg);
}

void amiga_ctl_init(amiga_ctl_t *ctl, config_nio_state_t *state,
                    uint8_t kick13, uint8_t rows, amiga_exec_fn exec,
                    void *exec_ctx)
{
  memset(ctl, 0, sizeof(*ctl));
  ctl->state = state;
  ctl->kick13 = kick13;
  ctl->exec = exec;
  ctl->exec_ctx = exec_ctx;
  ctl->fmount = AMIGA_DEFAULT_FMOUNT;
  ctl->fumount = AMIGA_DEFAULT_FUMOUNT;
  ctl->cat_base = AMIGA_CAT_SLOTS;
  amiga_list_init(&ctl->hosts, rows);
  amiga_list_init(&ctl->entries, rows);
  amiga_list_init(&ctl->catalogue, rows);
  amiga_list_init(&ctl->drives, rows);
  amiga_list_set_count(&ctl->hosts, state->host_count);
  amiga_list_set_count(&ctl->catalogue, AMIGA_CAT_SLOTS);
  amiga_list_set_count(&ctl->drives, AMIGA_DRIVE_COUNT);
}

void amiga_ctl_set_tools(amiga_ctl_t *ctl, const char *fmount,
                         const char *fumount)
{
  ctl->fmount = fmount;
  ctl->fumount = fumount;
}

void amiga_ctl_set_rows(amiga_ctl_t *ctl, uint8_t rows)
{
  amiga_list_set_rows(&ctl->hosts, rows);
  amiga_list_set_rows(&ctl->entries, rows);
  amiga_list_set_rows(&ctl->catalogue, rows);
  amiga_list_set_rows(&ctl->drives, rows);
}

void amiga_ctl_set_page(amiga_ctl_t *ctl, uint8_t page)
{
  if (page < AMIGA_PAGE_COUNT)
    ctl->page = page;
}

static int selected_host(amiga_ctl_t *ctl, uint8_t *index)
{
  if (ctl->hosts.selected == AMIGA_LIST_NONE ||
      ctl->hosts.selected >= ctl->state->host_count) {
    status(ctl, "No host selected");
    return 0;
  }
  *index = (uint8_t) ctl->hosts.selected;
  return 1;
}

static int save_hosts(amiga_ctl_t *ctl, const char *ok)
{
  amiga_list_set_count(&ctl->hosts, ctl->state->host_count);
  if (!config_nio_save_hosts(ctl->state)) {
    status(ctl, "Unable to save hosts");
    return 0;
  }
  status(ctl, ok);
  return 1;
}

int amiga_ctl_host_add(amiga_ctl_t *ctl, const char *uri)
{
  config_nio_state_t *s = ctl->state;

  if (!uri || !uri[0]) {
    status(ctl, "Host is empty");
    return 0;
  }
  if (s->host_count >= CONFIG_NIO_MAX_HOSTS) {
    status(ctl, "Host list is full");
    return 0;
  }
  (void) config_nio_host_set(s, s->host_count, uri);
  s->host_count++;
  amiga_list_set_count(&ctl->hosts, s->host_count);
  amiga_list_select(&ctl->hosts, (uint16_t) (s->host_count - 1));
  return save_hosts(ctl, "Host added");
}

int amiga_ctl_host_replace(amiga_ctl_t *ctl, const char *uri)
{
  uint8_t idx;

  if (!selected_host(ctl, &idx))
    return 0;
  if (!uri || !uri[0]) {
    status(ctl, "Host is empty");
    return 0;
  }
  (void) config_nio_host_set(ctl->state, idx, uri);
  return save_hosts(ctl, "Host updated");
}

int amiga_ctl_host_remove(amiga_ctl_t *ctl)
{
  config_nio_state_t *s = ctl->state;
  uint8_t idx, i;

  if (!selected_host(ctl, &idx))
    return 0;
  for (i = idx; (uint8_t) (i + 1) < s->host_count; i++)
    strcpy(s->hosts[i], s->hosts[i + 1]);
  s->host_count--;
  s->hosts[s->host_count][0] = 0;
  return save_hosts(ctl, "Host removed");
}

int amiga_ctl_host_move(amiga_ctl_t *ctl, int8_t delta)
{
  static char tmp[CONFIG_NIO_URI_MAX + 1];
  config_nio_state_t *s = ctl->state;
  uint8_t idx, to;

  if (!selected_host(ctl, &idx))
    return 0;
  if ((delta < 0 && idx == 0) ||
      (delta > 0 && (uint8_t) (idx + 1) >= s->host_count))
    return 0;
  to = (uint8_t) (delta < 0 ? idx - 1 : idx + 1);
  strcpy(tmp, s->hosts[idx]);
  strcpy(s->hosts[idx], s->hosts[to]);
  strcpy(s->hosts[to], tmp);
  amiga_list_select(&ctl->hosts, to);
  return save_hosts(ctl, "Host moved");
}

int amiga_ctl_set_prefs(amiga_ctl_t *ctl, uint8_t date_format,
                        uint8_t size_format)
{
  ctl->state->prefs.date_format = date_format;
  ctl->state->prefs.size_format = size_format;
  if (!config_nio_save_prefs(ctl->state)) {
    status(ctl, "Unable to save settings");
    return 0;
  }
  status(ctl, "Settings saved");
  return 1;
}
```

- [ ] **Step 5: Run to verify it passes**

Run: `make test-amiga-host`
Expected: every registered test passes, `0 failures`.

- [ ] **Step 6: Commit**

```bash
git add include/platform/amiga/amiga_ctl.h src/platform/amiga/amiga_ctl.c tests/amiga makefiles/test-amiga-host.mk
git commit -m "amiga: add controller host and settings operations with fake NIO

Verified with: make test-amiga-host"
```

---

### Task 8: Controller browsing, plus a larger entry table for Amiga

**Files:**
- Modify: `include/config_nio_state_embedded.h`. In the non-cc65 branch only, replace `#define CONFIG_NIO_MAX_ENTRIES 20` with:
  ```c
  #ifndef CONFIG_NIO_MAX_ENTRIES
  #define CONFIG_NIO_MAX_ENTRIES 20
  #endif
  ```
- Modify: `makefiles/test-amiga-host.mk` (append `-DCONFIG_NIO_MAX_ENTRIES=200` to `TEST_CFLAGS`), `tests/amiga/tests.def` (`AMIGA_TEST(ctl_browse)`)
- Modify: `include/platform/amiga/amiga_ctl.h`, `src/platform/amiga/amiga_ctl.c`
- Create: `tests/amiga/test_ctl_browse.c`

**Interfaces:**
- Consumes: `config_nio_compose_uri`, `config_nio_write_slot`, `fnsvc_list_directory`, `fnsvc_last_error`, `fnsvc_last_status`.
- Produces:
```c
int amiga_ctl_browse_open(amiga_ctl_t *ctl);        /* selected host, root path, page = BROWSE */
int amiga_ctl_browse_refresh(amiga_ctl_t *ctl);
int amiga_ctl_browse_activate(amiga_ctl_t *ctl);    /* enter selected drawer */
int amiga_ctl_browse_parent(amiga_ctl_t *ctl);
int amiga_ctl_browse_select_name(amiga_ctl_t *ctl, const char *name);
int amiga_ctl_browse_uri(amiga_ctl_t *ctl, char *out, uint16_t cap); /* selected file's URI */
int amiga_ctl_browse_assign(amiga_ctl_t *ctl, uint8_t slot, uint8_t readonly);
```

- [ ] **Step 1: Write the failing test** `tests/amiga/test_ctl_browse.c`

```c
#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;
static char many_names[250][8];
static fake_dir_entry_t many[250];

static int no_exec(const char *command, void *ctx)
{
  (void) command; (void) ctx;
  return 20;
}

void test_ctl_browse(void)
{
  static const fake_dir_entry_t root[] = {
    { CONFIG_NIO_ENTRY_FLAG_DIR, "GAMES", 0, 0 },
    { 0, "boot.adf", 901120, 1735689600UL },
    { CONFIG_NIO_ENTRY_FLAG_NAME_TRUNCATED, "very_long_na", 1, 1 },
  };
  static const fake_dir_entry_t games[] = { { 0, "a.adf", 880, 0 } };
  char uri[CONFIG_NIO_URI_MAX + 1];
  unsigned i;

  CHECK(CONFIG_NIO_MAX_ENTRIES == 200);
  fake_nio_reset();
  fake_dir_put("tnfs://fujinet.online/", root, 3);
  fake_dir_put("tnfs://fujinet.online/GAMES/", games, 1);
  fake_dir_fail("tnfs://fujinet.diller.org/");
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, no_exec, NULL);

  amiga_list_select(&ctl.hosts, 2);
  CHECK(amiga_ctl_browse_open(&ctl));
  CHECK(ctl.page == AMIGA_PAGE_BROWSE && ctl.browse_open);
  CHECK(ctl.entries.count == 3 && ctl.entries.selected == 0);
  CHECK_STR(state.status, "Entries loaded");

  CHECK(amiga_ctl_browse_select_name(&ctl, "boot.adf"));
  CHECK(amiga_ctl_browse_uri(&ctl, uri, sizeof(uri)));
  CHECK_STR(uri, "tnfs://fujinet.online/boot.adf");
  CHECK(!amiga_ctl_browse_activate(&ctl));
  CHECK_STR(state.status, "Choose a slot and press Assign");
  CHECK(amiga_ctl_browse_assign(&ctl, 12, 1));
  CHECK_STR(fake_slot_uri(12), "tnfs://fujinet.online/boot.adf");
  CHECK(fake_slot_readonly(12) == 1);
  CHECK_STR(state.status, "Assigned to slot 12");

  CHECK(amiga_ctl_browse_select_name(&ctl, "very_long_na"));
  CHECK(!amiga_ctl_browse_activate(&ctl));
  CHECK_STR(state.status, "Name too long for this client");
  CHECK(!amiga_ctl_browse_assign(&ctl, 13, 0));
  CHECK(fake_slot_uri(13) == NULL);

  CHECK(amiga_ctl_browse_select_name(&ctl, "GAMES"));
  CHECK(!amiga_ctl_browse_assign(&ctl, 13, 0));
  CHECK_STR(state.status, "Pick a file, not a drawer");
  CHECK(amiga_ctl_browse_activate(&ctl));
  CHECK_STR(state.browse_path, "GAMES/");
  CHECK(ctl.entries.count == 1);
  CHECK(amiga_ctl_browse_parent(&ctl));
  CHECK_STR(state.browse_path, "");
  CHECK(ctl.entries.count == 3);
  CHECK(!amiga_ctl_browse_parent(&ctl));
  CHECK(!amiga_ctl_browse_select_name(&ctl, "nope"));

  /* Unreachable host: page stays usable, nothing selectable. */
  amiga_list_select(&ctl.hosts, 1);
  CHECK(!amiga_ctl_browse_open(&ctl));
  CHECK(ctl.page == AMIGA_PAGE_BROWSE);
  CHECK(ctl.entries.count == 0);
  CHECK(!strncmp(state.status, "Browse failed: error ", 21));
  CHECK(!amiga_ctl_browse_activate(&ctl));
  CHECK_STR(state.status, "Nothing selected");

  /* Directory larger than the table. */
  for (i = 0; i < 250; i++) {
    sprintf(many_names[i], "F%03u", i);
    many[i].is_dir = 0;
    many[i].name = many_names[i];
    many[i].size = i;
    many[i].mtime = 0;
  }
  fake_dir_put("sd0:/", many, 250);
  amiga_list_select(&ctl.hosts, 0);
  CHECK(amiga_ctl_browse_open(&ctl));
  CHECK(ctl.entries.count == 200);
  CHECK_STR(state.status, "Showing 200 of 250 entries");

  /* No host at all. */
  while (state.host_count)
    CHECK(amiga_ctl_host_remove(&ctl));
  CHECK(!amiga_ctl_browse_open(&ctl));
  CHECK_STR(state.status, "No host selected");
}
```

The first host is `sd0:/`. `config_nio_compose_uri("sd0:/", "", "")` returns `"sd0:/"` because the host already has a prefix, which is why the fake directory key is `"sd0:/"`.

- [ ] **Step 2: Run to verify it fails**

Run: `make test-amiga-host ONLY=ctl_browse`
Expected: link errors (undefined `amiga_ctl_browse_open` …). The `CONFIG_NIO_MAX_ENTRIES == 200` check also fails until the header guard and flag are added.

- [ ] **Step 3: Implement**

Apply the header guard and the `-DCONFIG_NIO_MAX_ENTRIES=200` flag listed under **Files**. Then add the declarations to `amiga_ctl.h` and append to `amiga_ctl.c`:

```c
static void list_cb(uint8_t is_dir, const char *name, uint32_t size,
                    uint32_t mtime, void *ctx)
{
  config_nio_state_t *s = (config_nio_state_t *) ctx;
  config_nio_entry_t *entry;
  uint16_t n;

  s->entry_total++;
  if (s->entry_count >= CONFIG_NIO_MAX_ENTRIES) {
    s->entries_truncated = 1;
    return;
  }
  entry = &s->entries[s->entry_count++];
  entry->is_dir = is_dir;
  entry->size = size;
  entry->mtime = mtime;
  n = (uint16_t) strlen(name);
  if (n > CONFIG_NIO_NAME_MAX)
    n = CONFIG_NIO_NAME_MAX;
  memcpy(entry->name, name, n);
  entry->name[n] = 0;
}

int amiga_ctl_browse_refresh(amiga_ctl_t *ctl)
{
  static char uri[FNSVC_MAX_URI + 1];
  config_nio_state_t *s = ctl->state;

  s->entry_count = 0;
  s->entry_total = 0;
  s->entries_truncated = 0;
  ctl->browse_open = 1;
  amiga_list_set_count(&ctl->entries, 0);
  if (ctl->browse_host >= s->host_count ||
      !config_nio_compose_uri(s->hosts[ctl->browse_host], s->browse_path, "",
                              uri, sizeof(uri))) {
    status(ctl, "Path is too long");
    return 0;
  }
  if (!fnsvc_list_directory(uri, list_cb, s)) {
    s->entry_count = 0;
    sprintf(ctl->msg, "Browse failed: error %u status %u",
            (unsigned) fnsvc_last_error(), (unsigned) fnsvc_last_status());
    status(ctl, ctl->msg);
    return 0;
  }
  amiga_list_set_count(&ctl->entries, s->entry_count);
  amiga_list_select(&ctl->entries, 0);
  if (s->entries_truncated) {
    sprintf(ctl->msg, "Showing %u of %u entries",
            (unsigned) s->entry_count, (unsigned) s->entry_total);
    status(ctl, ctl->msg);
  } else {
    status(ctl, "Entries loaded");
  }
  return 1;
}

int amiga_ctl_browse_open(amiga_ctl_t *ctl)
{
  uint8_t idx;

  if (!selected_host(ctl, &idx))
    return 0;
  ctl->browse_host = idx;
  ctl->state->browse_path[0] = 0;
  ctl->page = AMIGA_PAGE_BROWSE;
  return amiga_ctl_browse_refresh(ctl);
}

static config_nio_entry_t *selected_entry(amiga_ctl_t *ctl)
{
  if (ctl->entries.selected == AMIGA_LIST_NONE ||
      ctl->entries.selected >= ctl->state->entry_count) {
    status(ctl, "Nothing selected");
    return NULL;
  }
  return &ctl->state->entries[ctl->entries.selected];
}

int amiga_ctl_browse_activate(amiga_ctl_t *ctl)
{
  config_nio_state_t *s = ctl->state;
  config_nio_entry_t *e;
  uint16_t len, nlen;

  e = selected_entry(ctl);
  if (!e)
    return 0;
  if (e->is_dir & CONFIG_NIO_ENTRY_FLAG_NAME_TRUNCATED) {
    status(ctl, "Name too long for this client");
    return 0;
  }
  if (!(e->is_dir & CONFIG_NIO_ENTRY_FLAG_DIR)) {
    status(ctl, "Choose a slot and press Assign");
    return 0;
  }
  len = (uint16_t) strlen(s->browse_path);
  nlen = (uint16_t) strlen(e->name);
  if ((uint16_t) (len + nlen + 2) > CONFIG_NIO_PATH_MAX) {
    status(ctl, "Path is too long");
    return 0;
  }
  memcpy(&s->browse_path[len], e->name, nlen);
  s->browse_path[len + nlen] = '/';
  s->browse_path[len + nlen + 1] = 0;
  return amiga_ctl_browse_refresh(ctl);
}

int amiga_ctl_browse_parent(amiga_ctl_t *ctl)
{
  char *path = ctl->state->browse_path;
  uint16_t len;

  len = (uint16_t) strlen(path);
  if (len == 0) {
    status(ctl, "Already at the top");
    return 0;
  }
  while (len > 0 && path[len - 1] == '/')
    path[--len] = 0;
  while (len > 0 && path[len - 1] != '/')
    path[--len] = 0;
  return amiga_ctl_browse_refresh(ctl);
}

int amiga_ctl_browse_select_name(amiga_ctl_t *ctl, const char *name)
{
  uint8_t i;

  for (i = 0; i < ctl->state->entry_count; i++) {
    if (!strcmp(ctl->state->entries[i].name, name)) {
      amiga_list_select(&ctl->entries, i);
      return 1;
    }
  }
  status(ctl, "No such entry");
  return 0;
}

int amiga_ctl_browse_uri(amiga_ctl_t *ctl, char *out, uint16_t cap)
{
  config_nio_entry_t *e = selected_entry(ctl);

  if (!e)
    return 0;
  if (e->is_dir & CONFIG_NIO_ENTRY_FLAG_NAME_TRUNCATED) {
    status(ctl, "Name too long for this client");
    return 0;
  }
  if (e->is_dir & CONFIG_NIO_ENTRY_FLAG_DIR) {
    status(ctl, "Pick a file, not a drawer");
    return 0;
  }
  if (!config_nio_compose_uri(ctl->state->hosts[ctl->browse_host],
                              ctl->state->browse_path, e->name, out, cap)) {
    status(ctl, "URI is too long");
    return 0;
  }
  return 1;
}

int amiga_ctl_browse_assign(amiga_ctl_t *ctl, uint8_t slot, uint8_t readonly)
{
  static char uri[FNSVC_MAX_URI + 1];

  if (!amiga_ctl_browse_uri(ctl, uri, sizeof(uri)))
    return 0;
  if (!config_nio_write_slot(ctl->state, slot, uri, readonly ? "r" : "rw")) {
    status(ctl, "Unable to save slot");
    return 0;
  }
  ctl->cat_base = AMIGA_CAT_SLOTS;
  sprintf(ctl->msg, "Assigned to slot %u", (unsigned) slot);
  status(ctl, ctl->msg);
  return 1;
}
```

- [ ] **Step 4: Run to verify it passes, and prove the header change is inert elsewhere**

Run:
```sh
make test-amiga-host
make TARGET=linux FUJINET_NIO_LIB=../fujinet-nio-lib
cmp build/linux/bin/config-nio ../../build/nio-config-baseline/config-nio.linux && echo linux-identical
```
Expected: `0 failures`, then `linux-identical`. If `cmp` differs, rebuild the baseline from `git stash`-ed sources to rule out a non-deterministic link before investigating.

- [ ] **Step 5: Commit**

```bash
git add include/config_nio_state_embedded.h include/platform/amiga/amiga_ctl.h src/platform/amiga/amiga_ctl.c tests/amiga makefiles/test-amiga-host.mk
git commit -m "amiga: add controller browsing and 200-entry Amiga directory table

The embedded header now lets a target override CONFIG_NIO_MAX_ENTRIES;
every existing target keeps 20 (linux binary byte-identical).

Verified with: make test-amiga-host; linux build cmp against baseline"
```

---

### Task 9: Controller catalogue

**Files:**
- Modify: `include/platform/amiga/amiga_ctl.h`, `src/platform/amiga/amiga_ctl.c`, `tests/amiga/tests.def` (`AMIGA_TEST(ctl_catalogue)`)
- Create: `tests/amiga/test_ctl_catalogue.c`

**Interfaces:**
- Consumes: `config_nio_read_slot`, `config_nio_write_slot`, `config_nio_delete_slot`.
- Produces:
```c
const config_nio_slot_t *amiga_ctl_slot(amiga_ctl_t *ctl, uint8_t slot); /* NULL on read error */
int amiga_ctl_slot_set(amiga_ctl_t *ctl, uint8_t slot, const char *uri, uint8_t readonly);
int amiga_ctl_slot_clear(amiga_ctl_t *ctl, uint8_t slot);
```
- Caching: slots are read in aligned windows of `AMIGA_CAT_WINDOW` (16). The GUI repaints ~10 visible rows, so a repaint costs at most 32 network reads, and 0 when the window is cached. Any write invalidates the cache.

- [ ] **Step 1: Write the failing test** `tests/amiga/test_ctl_catalogue.c`

```c
#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;

static int no_exec(const char *command, void *ctx)
{
  (void) command; (void) ctx;
  return 20;
}

void test_ctl_catalogue(void)
{
  const config_nio_slot_t *s;
  unsigned calls;

  fake_nio_reset();
  fake_slot_put(3, "tnfs://x/a.adf", 1);
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, no_exec, NULL);

  calls = fake_slot_get_calls();
  s = amiga_ctl_slot(&ctl, 3);
  CHECK(s && s->enabled && !strcmp(s->uri, "tnfs://x/a.adf") && !strcmp(s->mode, "r"));
  CHECK(fake_slot_get_calls() == calls + AMIGA_CAT_WINDOW);
  s = amiga_ctl_slot(&ctl, 10);
  CHECK(s && !s->enabled);
  CHECK(fake_slot_get_calls() == calls + AMIGA_CAT_WINDOW);
  CHECK(amiga_ctl_slot(&ctl, 16) != NULL);
  CHECK(fake_slot_get_calls() == calls + 2 * AMIGA_CAT_WINDOW);
  CHECK(amiga_ctl_slot(&ctl, 255) != NULL);

  CHECK(amiga_ctl_slot_set(&ctl, 20, "tnfs://x/b.adf", 0));
  CHECK_STR(fake_slot_uri(20), "tnfs://x/b.adf");
  CHECK(fake_slot_readonly(20) == 0);
  CHECK_STR(state.status, "Slot 20 saved");
  s = amiga_ctl_slot(&ctl, 20);
  CHECK(s && !strcmp(s->uri, "tnfs://x/b.adf") && !strcmp(s->mode, "rw"));

  CHECK(!amiga_ctl_slot_set(&ctl, 21, "", 0));
  CHECK_STR(state.status, "URI is empty");

  CHECK(amiga_ctl_slot_clear(&ctl, 3));
  CHECK(fake_slot_uri(3) == NULL);
  CHECK_STR(state.status, "Slot 3 cleared");
  s = amiga_ctl_slot(&ctl, 3);
  CHECK(s && !s->enabled);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test-amiga-host ONLY=ctl_catalogue`
Expected: undefined references to `amiga_ctl_slot` etc.

- [ ] **Step 3: Implement** (append to `amiga_ctl.c`; declarations to the header)

```c
const config_nio_slot_t *amiga_ctl_slot(amiga_ctl_t *ctl, uint8_t slot)
{
  uint16_t base;
  uint8_t i;

  base = (uint16_t) (slot & ~(AMIGA_CAT_WINDOW - 1));
  if (ctl->cat_base != base) {
    for (i = 0; i < AMIGA_CAT_WINDOW; i++) {
      /* config_nio_read_slot: 1 = entry present, or missing (zeroed);
       * 0 = transport/format error. */
      if (!config_nio_read_slot((uint8_t) (base + i), &ctl->cat[i])) {
        ctl->cat_base = AMIGA_CAT_SLOTS;
        status(ctl, "Unable to read catalogue");
        return NULL;
      }
    }
    ctl->cat_base = base;
  }
  return &ctl->cat[slot - base];
}
```

```c
int amiga_ctl_slot_set(amiga_ctl_t *ctl, uint8_t slot, const char *uri,
                       uint8_t readonly)
{
  if (!uri || !uri[0]) {
    status(ctl, "URI is empty");
    return 0;
  }
  ctl->cat_base = AMIGA_CAT_SLOTS;
  if (!config_nio_write_slot(ctl->state, slot, uri, readonly ? "r" : "rw")) {
    status(ctl, "Unable to save slot");
    return 0;
  }
  sprintf(ctl->msg, "Slot %u saved", (unsigned) slot);
  status(ctl, ctl->msg);
  return 1;
}

int amiga_ctl_slot_clear(amiga_ctl_t *ctl, uint8_t slot)
{
  ctl->cat_base = AMIGA_CAT_SLOTS;
  if (!config_nio_delete_slot(ctl->state, slot)) {
    status(ctl, "Unable to clear slot");
    return 0;
  }
  sprintf(ctl->msg, "Slot %u cleared", (unsigned) slot);
  status(ctl, ctl->msg);
  return 1;
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test-amiga-host`
Expected: `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add include/platform/amiga/amiga_ctl.h src/platform/amiga/amiga_ctl.c tests/amiga
git commit -m "amiga: add cached catalogue reads and slot set/clear

Verified with: make test-amiga-host"
```

---

### Task 10: Controller drives (FMOUNT/FUMOUNT with verification)

**Files:**
- Modify: `include/platform/amiga/amiga_ctl.h`, `src/platform/amiga/amiga_ctl.c`, `tests/amiga/tests.def` (`AMIGA_TEST(ctl_drives)`)
- Create: `tests/amiga/test_ctl_drives.c`

**Interfaces:**
- Consumes: `amiga_fmount_command`, `amiga_fumount_command`, `amiga_drive_label` (Task 1); `amiga_ctl_slot` (Task 9); `config_nio_load`; `config_nio_mapping_get`.
- Produces:
```c
int amiga_ctl_drive_insert(amiga_ctl_t *ctl, uint8_t unit, uint8_t slot, uint8_t readonly);
int amiga_ctl_drive_eject(amiga_ctl_t *ctl, uint8_t unit);
int amiga_ctl_reload(amiga_ctl_t *ctl);   /* re-read hosts/mappings/prefs, keep browse path */
```

- [ ] **Step 1: Write the failing test** `tests/amiga/test_ctl_drives.c`

```c
#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"
#include "amiga_drives.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;
static char last_cmd[AMIGA_CMD_MAX];
static int exec_calls;
static int exec_rc;
static int exec_records;

/* Behaves like FMOUNT/FUMOUNT + fujinet-disk.device: a successful command
 * rewrites config-nio/mappings (version byte + 8 x {flags, slot}). */
static int fake_exec(const char *cmd, void *ctx)
{
  uint8_t map[17];
  const uint8_t *cur;
  uint16_t len;
  unsigned slot;
  char label[8], mode[4];
  int unit;

  (void) ctx;
  exec_calls++;
  strcpy(last_cmd, cmd);
  memset(map, 0, sizeof(map));
  map[0] = 1;
  cur = fake_appstore_get("config-nio", "mappings", &len);
  if (cur && len == sizeof(map))
    memcpy(map, cur, sizeof(map));
  if (sscanf(cmd, "SYS:C/fmount %u %7s %3s", &slot, label, mode) == 3) {
    unit = amiga_drive_unit(label, 0);
    if (exec_records && unit >= 0) {
      map[1 + unit * 2] = (uint8_t) (1 | (mode[1] == 'O' ? 2 : 0));
      map[2 + unit * 2] = (uint8_t) slot;
    }
  } else if (sscanf(cmd, "SYS:C/fumount %d", &unit) == 1) {
    if (exec_records && unit >= 0 && unit < 8) {
      map[1 + unit * 2] = 0;
      map[2 + unit * 2] = 0;
    }
  }
  fake_appstore_put("config-nio", "mappings", map, sizeof(map));
  return exec_rc;
}

void test_ctl_drives(void)
{
  config_nio_mapping_t m;

  fake_nio_reset();
  fake_slot_put(12, "tnfs://x/boot.adf", 0);
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, fake_exec, NULL);
  exec_calls = 0;
  exec_rc = 0;
  exec_records = 1;
  strcpy(state.browse_path, "GAMES/");

  CHECK(amiga_ctl_drive_insert(&ctl, 0, 12, 1));
  CHECK_STR(last_cmd, "SYS:C/fmount 12 DN0: RO");
  CHECK(config_nio_mapping_get(&state, 0, &m) && m.valid && m.slot == 12 && m.readonly);
  CHECK_STR(state.status, "Slot 12 inserted in DN0:");
  CHECK_STR(state.browse_path, "GAMES/");
  CHECK(state.host_count == 3 && ctl.hosts.count == 3);

  CHECK(!amiga_ctl_drive_insert(&ctl, 1, 13, 0));
  CHECK_STR(state.status, "Catalogue slot 13 is empty");
  CHECK(exec_calls == 1);

  exec_rc = 10;
  exec_records = 0;
  CHECK(!amiga_ctl_drive_insert(&ctl, 2, 12, 0));
  CHECK_STR(state.status, "FMOUNT failed for DN2: (rc 10)");

  /* KS1.3 Execute() reports rc 0 even when FMOUNT failed: the missing
   * mapping must still be reported as a failure. */
  ctl.kick13 = 1;
  exec_rc = 0;
  CHECK(!amiga_ctl_drive_insert(&ctl, 2, 12, 0));
  CHECK_STR(last_cmd, "SYS:C/fmount 12 HN0: RW");
  CHECK_STR(state.status, "FMOUNT failed for HN0: (rc 0)");
  ctl.kick13 = 0;

  exec_records = 1;
  CHECK(amiga_ctl_drive_eject(&ctl, 0));
  CHECK_STR(last_cmd, "SYS:C/fumount 0");
  CHECK(config_nio_mapping_get(&state, 0, &m) && !m.valid);
  CHECK_STR(state.status, "DN0: ejected");
  CHECK(!amiga_ctl_drive_eject(&ctl, 0));
  CHECK_STR(state.status, "DN0: is empty");

  CHECK(!amiga_ctl_drive_insert(&ctl, 8, 12, 0));
  CHECK_STR(state.status, "No such drive");

  amiga_ctl_set_tools(&ctl, "Work:My Tools/fmount", "SYS:C/fumount");
  CHECK(!amiga_ctl_drive_insert(&ctl, 1, 12, 0));
  CHECK_STR(state.status, "FMOUNT path is invalid");
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test-amiga-host ONLY=ctl_drives`
Expected: undefined references to `amiga_ctl_drive_insert` etc.

- [ ] **Step 3: Implement** (append to `amiga_ctl.c`; declarations to the header)

```c
int amiga_ctl_reload(amiga_ctl_t *ctl)
{
  static char path[CONFIG_NIO_PATH_MAX + 1];
  config_nio_state_t *s = ctl->state;

  strcpy(path, s->browse_path);
  if (!config_nio_load(s)) {
    status(ctl, "Unable to reload FujiNet state");
    return 0;
  }
  strcpy(s->browse_path, path);
  /* config_nio_load() clears the directory listing; Browse re-reads it. */
  ctl->browse_open = 0;
  amiga_list_set_count(&ctl->entries, 0);
  amiga_list_set_count(&ctl->hosts, s->host_count);
  ctl->cat_base = AMIGA_CAT_SLOTS;
  return 1;
}

int amiga_ctl_drive_insert(amiga_ctl_t *ctl, uint8_t unit, uint8_t slot,
                           uint8_t readonly)
{
  const config_nio_slot_t *entry;
  const char *label;
  config_nio_mapping_t m;
  int rc;

  label = amiga_drive_label(unit, ctl->kick13);
  if (!label) {
    status(ctl, "No such drive");
    return 0;
  }
  entry = amiga_ctl_slot(ctl, slot);
  if (!entry)
    return 0;
  if (!entry->enabled || !entry->uri[0]) {
    sprintf(ctl->msg, "Catalogue slot %u is empty", (unsigned) slot);
    status(ctl, ctl->msg);
    return 0;
  }
  if (!amiga_fmount_command(ctl->cmd, sizeof(ctl->cmd), ctl->fmount, slot,
                            unit, ctl->kick13, readonly)) {
    status(ctl, "FMOUNT path is invalid");
    return 0;
  }
  rc = ctl->exec(ctl->cmd, ctl->exec_ctx);
  if (!amiga_ctl_reload(ctl))
    return 0;
  if (rc != 0 || !config_nio_mapping_get(ctl->state, unit, &m) ||
      !m.valid || m.slot != slot) {
    sprintf(ctl->msg, "FMOUNT failed for %s (rc %d)", label, rc);
    status(ctl, ctl->msg);
    return 0;
  }
  sprintf(ctl->msg, "Slot %u inserted in %s", (unsigned) slot, label);
  status(ctl, ctl->msg);
  return 1;
}

int amiga_ctl_drive_eject(amiga_ctl_t *ctl, uint8_t unit)
{
  const char *label;
  config_nio_mapping_t m;
  int rc;

  label = amiga_drive_label(unit, ctl->kick13);
  if (!label) {
    status(ctl, "No such drive");
    return 0;
  }
  if (!config_nio_mapping_get(ctl->state, unit, &m) || !m.valid) {
    sprintf(ctl->msg, "%s is empty", label);
    status(ctl, ctl->msg);
    return 0;
  }
  if (!amiga_fumount_command(ctl->cmd, sizeof(ctl->cmd), ctl->fumount, unit,
                             ctl->kick13)) {
    status(ctl, "FUMOUNT path is invalid");
    return 0;
  }
  rc = ctl->exec(ctl->cmd, ctl->exec_ctx);
  if (!amiga_ctl_reload(ctl))
    return 0;
  if (rc != 0 || !config_nio_mapping_get(ctl->state, unit, &m) || m.valid) {
    sprintf(ctl->msg, "FUMOUNT failed for %s (rc %d)", label, rc);
    status(ctl, ctl->msg);
    return 0;
  }
  sprintf(ctl->msg, "%s ejected", label);
  status(ctl, ctl->msg);
  return 1;
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test-amiga-host`
Expected: `0 failures`.

- [ ] **Step 5: Commit**

```bash
git add include/platform/amiga/amiga_ctl.h src/platform/amiga/amiga_ctl.c tests/amiga
git commit -m "amiga: insert and eject drives via FMOUNT/FUMOUNT with mapping verification

Verified with: make test-amiga-host"
```

---

### Task 11: Script driver

**Files:**
- Create: `include/platform/amiga/amiga_script.h`, `src/platform/amiga/amiga_script.c`, `tests/amiga/test_script.c`
- Modify: `tests/amiga/tests.def` (`AMIGA_TEST(script)`), `makefiles/test-amiga-host.mk` (add `amiga_script.c`)

**Interfaces:**
- Consumes: every `amiga_ctl_*` operation above, plus `amiga_drive_unit` and `amiga_drive_label`.
- Produces:
```c
enum { AMIGA_SCRIPT_ERR = 0, AMIGA_SCRIPT_OK = 1, AMIGA_SCRIPT_QUIT = 2, AMIGA_SCRIPT_WAIT = 3 };
typedef void (*amiga_script_out_fn)(const char *line, void *ctx);
int amiga_script_line(amiga_ctl_t *ctl, const char *line, amiga_script_out_fn out, void *out_ctx,
                      uint16_t *wait_ticks);
```
- Command language. Tokens are separated by spaces, and URIs contain no spaces. Blank lines and lines starting with `;` are ignored. `ro|rw` is case-insensitive.

| Command | Action | Output |
| --- | --- | --- |
| `page hosts\|browse\|catalogue\|drives` | set page | `OK` |
| `host add URI` / `host edit URI` / `host remove` / `host up` / `host down` | host ops | `OK` or `ERR <status>` |
| `host select N` | select host N | `OK` / `ERR No host selected` |
| `browse` | open selected host | `OK` / `ERR …` |
| `select NAME` / `enter` / `parent` | browse ops | |
| `assign SLOT ro\|rw` | assign selected file | |
| `slot set N URI ro\|rw` / `slot clear N` | catalogue ops | |
| `insert SLOT DRIVE ro\|rw` / `eject DRIVE` | drive ops (DRIVE is a label like `DN0:`) | |
| `dump hosts` | `HOST <i> <uri>` per host | then `OK` |
| `dump entries` | `ENTRY <D\|F> <name> <size>` | then `OK` |
| `dump drives` | `DRIVE <label> <slot\|-> <RO\|RW\|->` | then `OK` |
| `dump slot N` | `SLOT <n> <RO\|RW> <uri>` or `SLOT <n> EMPTY` | then `OK` |
| `dump status` | `STATUS <text>` | then `OK` |
| `wait TICKS` | GUI pauses (1/50 s ticks) | returns WAIT, no output |
| `quit` | end | returns QUIT, no output |

An unknown command sets the status to `Unknown command` and prints `ERR Unknown command`. Wrong arguments print `ERR Bad arguments`.

- [ ] **Step 1: Write the failing test** `tests/amiga/test_script.c`

```c
#include "check.h"
#include "fake_nio.h"
#include "amiga_ctl.h"
#include "amiga_script.h"

static config_nio_state_t state;
static amiga_ctl_t ctl;
static char transcript[4096];

static void out(const char *line, void *ctx)
{
  (void) ctx;
  strcat(transcript, line);
  strcat(transcript, "\n");
}

static int ok_exec(const char *cmd, void *ctx)
{
  uint8_t map[17];
  unsigned slot;
  char label[8], mode[4];
  int unit;

  (void) ctx;
  memset(map, 0, sizeof(map));
  map[0] = 1;
  if (sscanf(cmd, "SYS:C/fmount %u %7s %3s", &slot, label, mode) == 3 &&
      (unit = amiga_drive_unit(label, 0)) >= 0) {
    map[1 + unit * 2] = (uint8_t) (1 | (mode[1] == 'O' ? 2 : 0));
    map[2 + unit * 2] = (uint8_t) slot;
  }
  fake_appstore_put("config-nio", "mappings", map, sizeof(map));
  return 0;
}

static int run(const char *line)
{
  uint16_t ticks = 0;
  return amiga_script_line(&ctl, line, out, NULL, &ticks);
}

void test_script(void)
{
  uint16_t ticks = 0;

  fake_nio_reset();
  CHECK(config_nio_load(&state));
  amiga_ctl_init(&ctl, &state, 0, 10, ok_exec, NULL);
  transcript[0] = 0;

  CHECK(run("page drives") == AMIGA_SCRIPT_OK && ctl.page == AMIGA_PAGE_DRIVES);
  CHECK(run("page nope") == AMIGA_SCRIPT_ERR);
  CHECK(run("host add tnfs://example") == AMIGA_SCRIPT_OK);
  CHECK(run("dump hosts") == AMIGA_SCRIPT_OK);
  CHECK(run("slot set 12 tnfs://x/boot.adf RO") == AMIGA_SCRIPT_OK);
  CHECK(run("dump slot 12") == AMIGA_SCRIPT_OK);
  CHECK(run("dump slot 13") == AMIGA_SCRIPT_OK);
  CHECK(run("insert 12 DN0: ro") == AMIGA_SCRIPT_OK);
  CHECK(run("dump drives") == AMIGA_SCRIPT_OK);
  CHECK(run("insert 12") == AMIGA_SCRIPT_ERR);
  CHECK(run("; comment") == AMIGA_SCRIPT_OK);
  CHECK(run("") == AMIGA_SCRIPT_OK);
  CHECK(run("frobnicate") == AMIGA_SCRIPT_ERR);
  CHECK(amiga_script_line(&ctl, "wait 50", out, NULL, &ticks) == AMIGA_SCRIPT_WAIT && ticks == 50);
  CHECK(run("quit") == AMIGA_SCRIPT_QUIT);

  CHECK_STR(transcript,
    "OK\n"
    "ERR Unknown page\n"
    "OK\n"
    "HOST 0 sd0:/\n"
    "HOST 1 fujinet.diller.org\n"
    "HOST 2 fujinet.online\n"
    "HOST 3 tnfs://example\n"
    "OK\n"
    "OK\n"
    "SLOT 12 RO tnfs://x/boot.adf\n"
    "OK\n"
    "SLOT 13 EMPTY\n"
    "OK\n"
    "OK\n"
    "DRIVE DN0: 12 RO\n"
    "DRIVE DN1: - -\n"
    "DRIVE DN2: - -\n"
    "DRIVE DN3: - -\n"
    "DRIVE DN4: - -\n"
    "DRIVE DN5: - -\n"
    "DRIVE DN6: - -\n"
    "DRIVE DN7: - -\n"
    "OK\n"
    "ERR Bad arguments\n"
    "ERR Unknown command\n");
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `make test-amiga-host ONLY=script`
Expected: compile error, missing `amiga_script.h`.

- [ ] **Step 3: Implement** `src/platform/amiga/amiga_script.c`

```c
#include "amiga_script.h"
#include "amiga_drives.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOKENS 5

static char line_buf[CONFIG_NIO_URI_MAX + 64];
static char out_buf[CONFIG_NIO_URI_MAX + 32];

static int split(char *s, char **tok)
{
  int n = 0;

  while (*s && n < MAX_TOKENS) {
    while (*s == ' ' || *s == '\t')
      *s++ = 0;
    if (!*s)
      break;
    tok[n++] = s;
    while (*s && *s != ' ' && *s != '\t')
      s++;
  }
  return n;
}

static int is(const char *a, const char *b)
{
  while (*a && *b) {
    char c = *a;
    if (c >= 'A' && c <= 'Z')
      c = (char) (c + 32);
    if (c != *b)
      return 0;
    a++;
    b++;
  }
  return *a == 0 && *b == 0;
}

static int parse_mode(const char *s, uint8_t *readonly)
{
  if (is(s, "ro")) { *readonly = 1; return 1; }
  if (is(s, "rw")) { *readonly = 0; return 1; }
  return 0;
}

static int parse_u8(const char *s, uint8_t *v)
{
  char *end;
  long n = strtol(s, &end, 10);
  if (!*s || *end || n < 0 || n > 255)
    return 0;
  *v = (uint8_t) n;
  return 1;
}

static int result(amiga_ctl_t *ctl, int ok, amiga_script_out_fn out, void *ctx)
{
  if (ok) {
    out("OK", ctx);
    return AMIGA_SCRIPT_OK;
  }
  sprintf(out_buf, "ERR %s", ctl->state->status);
  out(out_buf, ctx);
  return AMIGA_SCRIPT_ERR;
}

static int fail(amiga_ctl_t *ctl, const char *msg, amiga_script_out_fn out, void *ctx)
{
  config_nio_set_status(ctl->state, msg);
  return result(ctl, 0, out, ctx);
}

static int dump(amiga_ctl_t *ctl, char **t, int n, amiga_script_out_fn out, void *ctx)
{
  config_nio_state_t *s = ctl->state;
  uint8_t i;

  if (n == 2 && is(t[1], "hosts")) {
    for (i = 0; i < s->host_count; i++) {
      sprintf(out_buf, "HOST %u %s", (unsigned) i, s->hosts[i]);
      out(out_buf, ctx);
    }
  } else if (n == 2 && is(t[1], "entries")) {
    for (i = 0; i < s->entry_count; i++) {
      sprintf(out_buf, "ENTRY %c %s %lu",
              (s->entries[i].is_dir & CONFIG_NIO_ENTRY_FLAG_DIR) ? 'D' : 'F',
              s->entries[i].name, (unsigned long) s->entries[i].size);
      out(out_buf, ctx);
    }
  } else if (n == 2 && is(t[1], "drives")) {
    for (i = 0; i < AMIGA_DRIVE_COUNT; i++) {
      config_nio_mapping_t m;
      if (config_nio_mapping_get(s, i, &m) && m.valid)
        sprintf(out_buf, "DRIVE %s %u %s", amiga_drive_label(i, ctl->kick13),
                (unsigned) m.slot, m.readonly ? "RO" : "RW");
      else
        sprintf(out_buf, "DRIVE %s - -", amiga_drive_label(i, ctl->kick13));
      out(out_buf, ctx);
    }
  } else if (n == 3 && is(t[1], "slot") && parse_u8(t[2], &i)) {
    const config_nio_slot_t *slot = amiga_ctl_slot(ctl, i);
    if (!slot)
      return result(ctl, 0, out, ctx);
    if (slot->enabled && slot->uri[0])
      sprintf(out_buf, "SLOT %u %s %s", (unsigned) i,
              strcmp(slot->mode, "r") == 0 ? "RO" : "RW", slot->uri);
    else
      sprintf(out_buf, "SLOT %u EMPTY", (unsigned) i);
    out(out_buf, ctx);
  } else if (n == 2 && is(t[1], "status")) {
    sprintf(out_buf, "STATUS %s", s->status);
    out(out_buf, ctx);
  } else {
    return fail(ctl, "Bad arguments", out, ctx);
  }
  return result(ctl, 1, out, ctx);
}

int amiga_script_line(amiga_ctl_t *ctl, const char *line,
                      amiga_script_out_fn out, void *out_ctx,
                      uint16_t *wait_ticks)
{
  static const char *const pages[AMIGA_PAGE_COUNT] = {
    "hosts", "browse", "catalogue", "drives"
  };
  char *t[MAX_TOKENS];
  int n;
  uint8_t a, ro;
  int unit;

  strncpy(line_buf, line ? line : "", sizeof(line_buf) - 1);
  line_buf[sizeof(line_buf) - 1] = 0;
  n = split(line_buf, t);
  if (n == 0 || t[0][0] == ';')
    return AMIGA_SCRIPT_OK;

  if (is(t[0], "quit"))
    return AMIGA_SCRIPT_QUIT;
  if (is(t[0], "wait") && n == 2) {
    *wait_ticks = (uint16_t) atoi(t[1]);
    return AMIGA_SCRIPT_WAIT;
  }
  if (is(t[0], "page") && n == 2) {
    for (a = 0; a < AMIGA_PAGE_COUNT; a++) {
      if (is(t[1], pages[a])) {
        amiga_ctl_set_page(ctl, a);
        return result(ctl, 1, out, out_ctx);
      }
    }
    return fail(ctl, "Unknown page", out, out_ctx);
  }
  if (is(t[0], "host") && n >= 2) {
    if (is(t[1], "add") && n == 3) return result(ctl, amiga_ctl_host_add(ctl, t[2]), out, out_ctx);
    if (is(t[1], "edit") && n == 3) return result(ctl, amiga_ctl_host_replace(ctl, t[2]), out, out_ctx);
    if (is(t[1], "remove") && n == 2) return result(ctl, amiga_ctl_host_remove(ctl), out, out_ctx);
    if (is(t[1], "up") && n == 2) return result(ctl, amiga_ctl_host_move(ctl, -1), out, out_ctx);
    if (is(t[1], "down") && n == 2) return result(ctl, amiga_ctl_host_move(ctl, 1), out, out_ctx);
    if (is(t[1], "select") && n == 3 && parse_u8(t[2], &a)) {
      if (a >= ctl->state->host_count)
        return fail(ctl, "No host selected", out, out_ctx);
      amiga_list_select(&ctl->hosts, a);
      return result(ctl, 1, out, out_ctx);
    }
    return fail(ctl, "Bad arguments", out, out_ctx);
  }
  if (is(t[0], "browse") && n == 1) return result(ctl, amiga_ctl_browse_open(ctl), out, out_ctx);
  if (is(t[0], "select") && n == 2) return result(ctl, amiga_ctl_browse_select_name(ctl, t[1]), out, out_ctx);
  if (is(t[0], "enter") && n == 1) return result(ctl, amiga_ctl_browse_activate(ctl), out, out_ctx);
  if (is(t[0], "parent") && n == 1) return result(ctl, amiga_ctl_browse_parent(ctl), out, out_ctx);
  if (is(t[0], "assign") && n == 3 && parse_u8(t[1], &a) && parse_mode(t[2], &ro))
    return result(ctl, amiga_ctl_browse_assign(ctl, a, ro), out, out_ctx);
  if (is(t[0], "slot") && n == 5 && is(t[1], "set") && parse_u8(t[2], &a) && parse_mode(t[4], &ro))
    return result(ctl, amiga_ctl_slot_set(ctl, a, t[3], ro), out, out_ctx);
  if (is(t[0], "slot") && n == 3 && is(t[1], "clear") && parse_u8(t[2], &a))
    return result(ctl, amiga_ctl_slot_clear(ctl, a), out, out_ctx);
  if (is(t[0], "insert") && n == 4 && parse_u8(t[1], &a) && parse_mode(t[3], &ro)) {
    unit = amiga_drive_unit(t[2], ctl->kick13);
    if (unit < 0)
      return fail(ctl, "No such drive", out, out_ctx);
    return result(ctl, amiga_ctl_drive_insert(ctl, (uint8_t) unit, a, ro), out, out_ctx);
  }
  if (is(t[0], "eject") && n == 2) {
    unit = amiga_drive_unit(t[1], ctl->kick13);
    if (unit < 0)
      return fail(ctl, "No such drive", out, out_ctx);
    return result(ctl, amiga_ctl_drive_eject(ctl, (uint8_t) unit), out, out_ctx);
  }
  if (is(t[0], "dump"))
    return dump(ctl, t, n, out, out_ctx);
  if (is(t[0], "insert") || is(t[0], "eject") || is(t[0], "assign") ||
      is(t[0], "slot") || is(t[0], "page") || is(t[0], "wait"))
    return fail(ctl, "Bad arguments", out, out_ctx);
  return fail(ctl, "Unknown command", out, out_ctx);
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `make test-amiga-host`
Expected: `0 failures`, and no compiler warnings from `amiga_*.c`.

- [ ] **Step 5: Commit**

```bash
git add include/platform/amiga/amiga_script.h src/platform/amiga/amiga_script.c tests/amiga makefiles/test-amiga-host.mk
git commit -m "amiga: add SCRIPT= command driver for automated acceptance

Verified with: make test-amiga-host"
```

---

### Task 12: Amiga target build, process glue and window skeleton

**Files:**
- Modify: `makefiles/targets.mk`. Add these rows next to the existing ones:
  ```make
  PLATFORM_amiga := amiga
  COMPILER_FAMILY_amiga := amigagcc
  TOOLCHAIN_TARGET_amiga := amiga
  PROGRAM_EXT_amiga :=
  NIO_LIB_TARGET_amiga := amiga
  NIO_LIB_FILE_amiga := $(FUJINET_NIO_LIB)/build/fujinet-nio-amiga.a
  ```
  Then append ` amiga` to the supported-target list in the `$(error …)` text.
- Modify: `makefiles/build.mk`:
  1. Add `else ifeq ($(COMPILER_FAMILY),amigagcc)` / `include makefiles/compiler-amigagcc.mk` to the compiler chain.
  2. After the `rwildcard` source collection and the `/support/` and `/splash/` filters, add:
     ```make
     # Amiga has its own main() and Intuition front end. It links only the
     # portable state/store/table/service layers, not the text UI loops, plus
     # the platform-neutral app-store fnctl used by Linux.
     CONFIG_NIO_EXTRA_SRCS_amiga := \
     	$(SRC_DIR)/platform/portable/config_nio_state.c \
     	$(SRC_DIR)/platform/portable/config_nio_store.c \
     	$(SRC_DIR)/platform/portable/config_nio_tables.c \
     	$(SRC_DIR)/platform/portable/fnsvc.c \
     	$(SRC_DIR)/platform/linux/fnctl.c
     CONFIG_NIO_SRCS += $(CONFIG_NIO_EXTRA_SRCS_$(TARGET))
     ifeq ($(TARGET),amiga)
     CONFIG_NIO_SRCS := $(filter-out $(SRC_DIR)/main.c,$(CONFIG_NIO_SRCS))
     endif
     ```
- Create: `makefiles/compiler-amigagcc.mk`
- Modify: `Makefile`. Add `AMIGA_PROFILE ?= wb32` and the `amiga` rule. Do not add it to `TARGETS`.
- Create: `src/platform/amiga/amiga_stack.c`, `amiga_main.c`, `amiga_exec.c`, `amiga_gui.c` (skeleton), and `include/platform/amiga/amiga_exec.h`, `amiga_gui.h`

**Interfaces:**
- Consumes: `amiga_ctl_*`, `amiga_options_*`, `amiga_layout_compute`, `amiga_theme_*`.
- Produces:
  - `int amiga_exec_command(const char *command, void *ctx);`, which satisfies `amiga_exec_fn`.
  - `int amiga_gui_run(amiga_ctl_t *ctl, const amiga_options_t *opts);`, which returns the process rc.
  - `void amiga_gui_fatal(const char *message);`. The message may contain up to 3 lines separated by `\n`.
  - `void config_nio_fatal_message(const char *message);`, satisfying `config_nio.h`.

- [ ] **Step 1: Prove the target does not exist yet (the failing test)**

Run: `make amiga`
Expected: `make: *** No rule to make target 'amiga'`.

- [ ] **Step 2: Write the build files**

`makefiles/compiler-amigagcc.mk`. It matches `nio-core-apps/makefiles/compiler-amigagcc.mk` so both link the same way:
```make
CC := m68k-amigaos-gcc
AMIGA_CRT ?= clib2
FUJINET_NIO_DRIVER_BUILD ?= ../fujinet-nio-driver/build/amiga

CFLAGS += -Wall -Wextra -O2 -std=c99
CFLAGS += -mcpu=68000 -msoft-float
# Select the CRT's headers as well as its libraries (see nio-core-apps).
CFLAGS += -mcrt=$(AMIGA_CRT)
CFLAGS += -I$(APP_INCLUDE_DIR)
CFLAGS += -I$(CONFIG_NIO_INCLUDE_DIR)
CFLAGS += -I$(PLATFORM_INCLUDE_DIR)
CFLAGS += -I$(NIO_INCLUDE_DIR)
CFLAGS += -I$(FUJINET_NIO_DRIVER_BUILD)/include
CFLAGS += -DFNSVC_LIST_MAX_PAYLOAD=$(FNSVC_LIST_MAX_PAYLOAD)
CFLAGS += -DCONFIG_NIO_MAX_ENTRIES=200
CFLAGS += -D__AMIGA__

LDFLAGS += -mcpu=68000 -msoft-float -mcrt=$(AMIGA_CRT)
LDFLAGS += -L$(FUJINET_NIO_DRIVER_BUILD)/lib -lfujinet-amiga-disk

define compile_c
	$(CC) $(CFLAGS) -MMD -MF $(@:.o=.d) -c -o $@ $<
endef

define link_program
	$(CC) -o $@ $^ $(LDFLAGS) $(EXTRA_PROGRAM_LDFLAGS) -lamiga
endef
```

Top-level `Makefile` additions (add `amiga` to `.PHONY`):
```make
AMIGA_PROFILE ?= wb32
AMIGA_CRT_wb13 := nix13
AMIGA_CRT_wb31 := clib2
AMIGA_CRT_wb32 := clib2

amiga:
	$(MAKE) -f makefiles/build.mk TARGET=amiga \
		TARGET_BUILD_DIR=build/amiga/$(AMIGA_PROFILE) \
		AMIGA_CRT=$(AMIGA_CRT_$(AMIGA_PROFILE)) \
		FUJINET_NIO_DRIVER_BUILD=$(or $(FUJINET_NIO_DRIVER_BUILD),../fujinet-nio-driver/build/amiga/$(AMIGA_PROFILE))
```

`src/platform/amiga/amiga_stack.c`:
```c
/* Default Shell STACK is 4096; request more like the other NIO Amiga tools
 * (docs/amiga/cli-stack-and-iorequest.md). */
long __stack = 16384;
```

- [ ] **Step 3: Write `amiga_exec.c`**

```c
#include "amiga_exec.h"
#include "fujinet-nio.h"

#include <dos/dos.h>
#include <proto/dos.h>
#ifndef __KICK13__
#include <dos/dostags.h>
#endif

int amiga_exec_command(const char *command, void *ctx)
{
  BPTR out;
  LONG rc;

  (void) ctx;
  /* Mirror FMOUNTRESTORE: release this process's FujiNet session while a
   * child command opens its own, then reconnect so the caller can reload. */
  fn_shutdown();
  out = Open("NIL:", MODE_NEWFILE);
#ifdef __KICK13__
  /* KS1.3 Execute() reports only whether the command started; amiga_ctl
   * verifies the resulting mapping rather than trusting this value. */
  rc = Execute((STRPTR) command, 0, out) ? 0 : 20;
#else
  {
    BPTR in = Open("NIL:", MODE_OLDFILE);
    rc = SystemTags((CONST_STRPTR) command, SYS_Input, in, SYS_Output, out,
                    TAG_DONE);
    if (in)
      Close(in);
  }
#endif
  if (out)
    Close(out);
  if (fn_init() != FN_OK)
    return rc ? (int) rc : 20;
  return (int) rc;
}
```
`include/platform/amiga/amiga_exec.h` declares `int amiga_exec_command(const char *command, void *ctx);`.

- [ ] **Step 4: Write `amiga_main.c`**

```c
#include "config_nio.h"
#include "fujinet-nio.h"
#include "amiga_ctl.h"
#include "amiga_exec.h"
#include "amiga_gui.h"
#include "amiga_options.h"

#include <exec/types.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>

#include <stdio.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *IconBase;

static config_nio_state_t state;
static amiga_ctl_t ctl;
static amiga_options_t options;

#ifdef __KICK13__
#define AMIGA_KICK13 1
#else
#define AMIGA_KICK13 0
#endif

void config_nio_fatal_message(const char *message)
{
  amiga_gui_fatal(message);
}

static void read_tooltypes(struct WBStartup *wb)
{
  struct WBArg *arg;
  struct DiskObject *dobj;
  BPTR old;
  char **tt;

  if (!IconBase || !wb || wb->sm_NumArgs < 1)
    return;
  arg = &wb->sm_ArgList[0];
  old = CurrentDir(arg->wa_Lock);
  dobj = GetDiskObject(arg->wa_Name);
  if (dobj) {
    /* Workbench convention: unknown or malformed ToolTypes are ignored. */
    for (tt = (char **) dobj->do_ToolTypes; tt && *tt; tt++)
      (void) amiga_options_parse(&options, *tt);
    FreeDiskObject(dobj);
  }
  (void) CurrentDir(old);
}

int main(int argc, char **argv)
{
  int i;
  int rc = 20;

  IntuitionBase = (struct IntuitionBase *) OpenLibrary("intuition.library", 33);
  GfxBase = (struct GfxBase *) OpenLibrary("graphics.library", 33);
  IconBase = OpenLibrary("icon.library", 33);
  if (!IntuitionBase || !GfxBase)
    goto out;

  amiga_options_defaults(&options);
  if (argc == 0) {
    read_tooltypes((struct WBStartup *) argv);
  } else {
    for (i = 1; i < argc; i++) {
      if (amiga_options_parse(&options, argv[i]) != 1) {
        puts("Usage: config-nio [FMOUNT=path] [FUMOUNT=path] [SCRIPT=file] [RESULT=file]");
        rc = 10;
        goto out;
      }
    }
  }
  amiga_options_finish(&options);

  if (fn_init() != FN_OK) {
    amiga_gui_fatal("FujiNet init failed.\nIs fujinet-nio.device installed?");
    goto out;
  }
  if (!fn_is_ready()) {
    amiga_gui_fatal("FujiNet is not ready.");
    goto shutdown;
  }
  if (!config_nio_load(&state)) {
    amiga_gui_fatal("Unable to load config-nio state.");
    goto shutdown;
  }
  amiga_ctl_init(&ctl, &state, AMIGA_KICK13, 8, amiga_exec_command, NULL);
  amiga_ctl_set_tools(&ctl, options.fmount, options.fumount);
  rc = amiga_gui_run(&ctl, &options);

shutdown:
  fn_shutdown();
out:
  if (IconBase) CloseLibrary(IconBase);
  if (GfxBase) CloseLibrary((struct Library *) GfxBase);
  if (IntuitionBase) CloseLibrary((struct Library *) IntuitionBase);
  return rc;
}
```

Under both libnix (nix13) and clib2, a Workbench launch gives `argc == 0` and `argv` pointing to the `struct WBStartup`. Confirm this against the CRT headers during this step. If clib2 differs, use its documented `__WBenchMsg` symbol instead, and note it in the commit.

- [ ] **Step 5: Write the `amiga_gui.c` skeleton**

This covers: fatal requester, window open/close, layout, page buttons drawn, and the close gadget. Task 13 replaces the body with the full GUI. The skeleton must already do these things:
- Read the Workbench screen with `GetScreenData(&wb, sizeof(wb), WBENCHSCREEN, NULL)`. This works on 1.3 and later.
- Estimate the borders:
  - `border_t = wb.WBorTop + wb.Font->ta_YSize + 1`
  - `border_l = wb.WBorLeft`
  - `border_r = wb.WBorRight`
  - `border_b = wb.WBorBottom`
- Compute the layout with `font_w = 8`, `font_h = 8`, and `gadget_font_h = wb.Font->ta_YSize`.
- Open a centred `NewWindow` on `WBENCHSCREEN`:
  - Flags: `WINDOWDRAG | WINDOWDEPTH | WINDOWCLOSE | ACTIVATE | SMART_REFRESH | NOCAREREFRESH`.
  - IDCMP: `CLOSEWINDOW | GADGETUP | GADGETDOWN | MOUSEMOVE | RAWKEY | MENUPICK | NEWSIZE`.
  - Title: `"FujiNet Config"`.
- **Recompute the layout from the real `win->BorderLeft/Top/Right/Bottom`.** This reads what the OS drew instead of assuming one Kickstart's metrics. If the outer size differs, call `SizeWindow(win, dw, dh)` and wait for `NEWSIZE`.
- Open topaz 8 (`OpenFont` with `{"topaz.font", 8, 0, 0}`), call `SetFont(win->RPort, font)`, and draw the four page buttons and the status line.
- `amiga_gui_fatal()`: if `IntuitionBase` is NULL, `puts()` the message. Otherwise split it on `\n` into up to three chained `IntuiText`s (topaz 8, pen 0/1, `JAM1`) and call `AutoRequest(NULL, &body, NULL, &ok, 0, 0, 320, 40 + 10 * lines)`, where `ok` says `"OK"`.
- If `amiga_layout_compute()` returns 0, call `amiga_gui_fatal("The Workbench screen is too small.\nFujiNet Config needs 640x200.")` and return 20.

- [ ] **Step 6: Build both profiles**

Run:
```sh
source ../../scripts/env.sh
../../scripts/amiga-artifacts wb32 >/dev/null
../../scripts/amiga-artifacts wb13 >/dev/null
make amiga AMIGA_PROFILE=wb32
make amiga AMIGA_PROFILE=wb13
ls -l build/amiga/wb32/bin/config-nio build/amiga/wb13/bin/config-nio
make test-amiga-host
```
Expected: both links succeed with no warnings from `src/platform/amiga/`, both binaries exist, and host tests still show `0 failures`. If the wb13 link reports an unresolved symbol from `-lfujinet-amiga-disk` being unnecessary or CRT-specific, drop that `-l` flag. Keep it only if `nio-core-apps` needs the same flag for its wb13 build.

- [ ] **Step 7: Smoke-check the skeleton in the interactive Workbench**

Run: `../../scripts/build.sh amiga-workbench --profile wb32-a1200 -- --external-nio` (with FujiNet NIO listening on TCP 65504). Then run `NIO:config-nio` from the guest Shell, having first copied `build/amiga/wb32/bin/config-nio` into the staged `NIO:` directory: `build/amiga-artifacts/wb32/NIO/`.
Expected: the window opens centred with its four page buttons, and the close gadget exits with rc 0. Record a screenshot as evidence.

- [ ] **Step 8: Commit**

```bash
git add Makefile makefiles/targets.mk makefiles/build.mk makefiles/compiler-amigagcc.mk src/platform/amiga include/platform/amiga
git commit -m "amiga: add TARGET=amiga build, process glue and window skeleton

Verified with: make amiga AMIGA_PROFILE=wb32; make amiga AMIGA_PROFILE=wb13;
make test-amiga-host; manual WB3.2 window open/close"
```

---

### Task 13: Full Intuition interface

**Files:**
- Modify: `src/platform/amiga/amiga_gui.c`

**Interfaces:**
- Consumes: every `amiga_ctl_*` and helper module above. Produces nothing new; `amiga_gui_run` keeps its signature.

The following is the contract for this file. Every bullet is a required behaviour, and the reviewer checks each one.

**Gadgets.** Use static V33 structures and old-style names.
```c
enum { GID_TAB0 = 1, GID_LIST = GID_TAB0 + AMIGA_TAB_COUNT, GID_PROP, GID_EDIT,
       GID_SLOT, GID_RO, GID_BTN0 };
#define GID_COUNT (GID_BTN0 + AMIGA_BUTTON_COUNT)

enum { ACT_NONE = 0, ACT_HOST_BROWSE, ACT_HOST_ADD, ACT_HOST_REPLACE,
       ACT_HOST_REMOVE, ACT_HOST_UP, ACT_HOST_DOWN, ACT_BROWSE_OPEN,
       ACT_BROWSE_PARENT, ACT_BROWSE_REFRESH, ACT_BROWSE_ASSIGN,
       ACT_SLOT_SET, ACT_SLOT_CLEAR, ACT_DRIVE_INSERT, ACT_DRIVE_EJECT };

typedef struct { const char *label; uint8_t action; } gui_button_t;

static const char *const tab_labels[AMIGA_TAB_COUNT] = {
  "Hosts", "Browse", "Catalogue", "Drives"
};

static const gui_button_t page_buttons[AMIGA_PAGE_COUNT][AMIGA_BUTTON_COUNT] = {
  { { "Browse", ACT_HOST_BROWSE }, { "Add", ACT_HOST_ADD },
    { "Replace", ACT_HOST_REPLACE }, { "Remove", ACT_HOST_REMOVE },
    { "Move Up", ACT_HOST_UP }, { "Move Down", ACT_HOST_DOWN } },
  { { "Open", ACT_BROWSE_OPEN }, { "Parent", ACT_BROWSE_PARENT },
    { "Refresh", ACT_BROWSE_REFRESH }, { "Assign", ACT_BROWSE_ASSIGN },
    { NULL, ACT_NONE }, { NULL, ACT_NONE } },
  { { "Set", ACT_SLOT_SET }, { "Clear", ACT_SLOT_CLEAR },
    { NULL, ACT_NONE }, { NULL, ACT_NONE }, { NULL, ACT_NONE }, { NULL, ACT_NONE } },
  { { "Insert", ACT_DRIVE_INSERT }, { "Eject", ACT_DRIVE_EJECT },
    { NULL, ACT_NONE }, { NULL, ACT_NONE }, { NULL, ACT_NONE }, { NULL, ACT_NONE } },
};
```
- Tabs and buttons: `BOOLGADGET`, `GADGHCOMP`, `RELVERIFY`. The program draws their bevel and label. A button with a NULL label draws as empty background, and its click is ignored.
- List: a `BOOLGADGET` with `GADGHNONE` and `GADGIMMEDIATE` covering the list interior (`amiga_rect_inset(list, 2, 2)`). On `GADGETDOWN`, call `amiga_list_hit(page_list, msg->MouseY - interior.top, layout.row_h, &i)`. A second click on the same index within `DoubleClick()` time activates it.
- Scroller: `PROPGADGET` with `AUTOKNOB | FREEVERT`. Add `PROPNEWLOOK` only when `IntuitionBase->LibNode.lib_Version >= 36`, and never under `__KICK13__`. Activation is `GADGIMMEDIATE | RELVERIFY | FOLLOWMOUSE`. On `MOUSEMOVE`/`GADGETUP`, call `amiga_list_set_top_from_pot(list, pi.VertPot)` and repaint the rows only. After any list change, call `NewModifyProp(&prop, win, NULL, AUTOKNOB|FREEVERT[|PROPNEWLOOK], 0, pot, 0, body, 1)` with the values from `amiga_list_prop`.
- Edit: `STRGADGET`, with its buffer `CONFIG_NIO_URI_MAX + 1` characters long. Slot: `STRGADGET` with `LONGINT` and a 4-character buffer. RO: `BOOLGADGET` with `TOGGLESELECT | RELVERIFY` and `GADGHNONE`; the program draws a bevelled box and a check mark (two `Draw()` strokes in `theme.text`) when `SELECTED` is set.
- Page membership. Hosts shows Edit only. Browse shows Slot and RO. Catalogue and Drives show all three. Switch with `RemoveGList`/`AddGList` and then `RefreshGList`. Clear the area of a removed gadget to `theme.background` first.
- Setting string contents: `RemoveGadget` → copy the text → reset `BufferPos`/`DispPos` (and `LongInt` for Slot) → `AddGadget` at the same position → `RefreshGList(g, win, NULL, 1)`.

**Rendering.** Draw everything in topaz 8 with `JAM1` unless stated otherwise.
- `bevel(rp, r, recessed)`: 2.0 hires style, with a double left edge and a single top edge. Raised uses `shine` top/left and `shadow` bottom/right; recessed swaps them.
- Page buttons: the selected one is recessed, filled with `theme.fill`, with `theme.filltext` label; the others are raised on `theme.background` with `theme.text`. All labels are centred with `TextLength`.
- Info line:
  - Hosts: `FujiNet hosts (N of 8)`.
  - Browse: `amiga_clip_tail` of `host + "/" + browse_path`, or `Choose a host and press Browse` when `!ctl->browse_open`.
  - Catalogue: `Slot Mode Image`.
  - Drives: `Drive Slot Mode Image`.
- List rows. Here `cols = interior.width / 8`. The selected row is filled with `theme.fill` and drawn in `theme.filltext`; other rows are filled with `theme.background` and drawn in `theme.text`. Pad each row's text to `cols` characters so no stale pixels remain.
  - Hosts: `sprintf("%2u %s")` with `amiga_clip_head` applied to the host.
  - Browse: name `amiga_clip_head` to `cols - 24`. Right side: `Drawer` for directories, else `amiga_format_size` right-aligned in 13 columns, then a space and `amiga_format_date` (8).
  - Catalogue: `"%3u %s %s"` = slot, `RO`/`RW` or two spaces, and the clipped URI. A NULL from `amiga_ctl_slot` shows `?` and leaves the status message set.
  - Drives: `"%-5s"` label, then either `"%3u %s %s"` (slot, mode, clipped URI from `amiga_ctl_slot`) or `(empty)`.
- Labels `URI`, `Slot` and `RO` go left of or right of their gadgets, inside the label space the layout reserved.
- The status line is recessed and shows `ctl->state->status` clipped with `amiga_clip_head`.
- Pens: `amiga_theme_classic` by default. Under `#ifndef __KICK13__` with V36+, use `GetScreenDrawInfo(win->WScreen)` → `amiga_theme_from_pens(&theme, dri->dri_Pens, dri->dri_NumPens)` → `FreeScreenDrawInfo`.

**Selection to editor sync** (runs after every selection change):
- Hosts: Edit ← the selected host.
- Catalogue: Slot ← the row index; Edit ← its URI; RO ← `mode == "r"`.
- Drives: if the drive is mapped, Slot ← the mapped slot and RO ← the mapped mode.
- Browse: nothing changes; Slot and RO keep the user's target.

**Actions** (button or key). Each network or command action runs inside `busy_begin()`/`busy_end()`, then the program repaints the list, info line and status.

| Action | Call |
| --- | --- |
| HOST_BROWSE, or double-click / Return on Hosts | `amiga_ctl_browse_open`, which switches to Browse |
| HOST_ADD / HOST_REPLACE | `amiga_ctl_host_add/replace(ctl, edit_buf)` |
| HOST_REMOVE | `confirm("Remove this host?")` then `amiga_ctl_host_remove` |
| HOST_UP / HOST_DOWN | `amiga_ctl_host_move(±1)` |
| BROWSE_OPEN, or double-click / Return on Browse | `amiga_ctl_browse_activate` |
| BROWSE_PARENT, or Backspace | `amiga_ctl_browse_parent` |
| BROWSE_REFRESH | `amiga_ctl_browse_refresh`. If `!browse_open`, run `amiga_ctl_browse_open` instead |
| BROWSE_ASSIGN | read Slot (0–255, otherwise status `Slot must be 0-255`) and RO, then `amiga_ctl_browse_assign` |
| SLOT_SET | `amiga_ctl_slot_set(slot, edit_buf, ro)` |
| SLOT_CLEAR | `confirm("Clear this catalogue slot?")` then `amiga_ctl_slot_clear` |
| DRIVE_INSERT, or Return on Drives | `amiga_ctl_drive_insert(ctl->drives.selected, slot, ro)` |
| DRIVE_EJECT | `confirm("Eject <label>?")` then `amiga_ctl_drive_eject` |

- On the Catalogue page, a Slot `GADGETUP` (Return in the integer gadget) jumps the list to that slot.
- `confirm()` calls `AutoRequest(win, &body, &yes, &no, 0, 0, 280, 60)` with `yes = "Yes"` on the left and `no = "No"` on the right.

**Busy state:**
```c
/* RKM wait pointer image; sprite data must live in chip RAM. */
static const UWORD busy_image[] = {
  0x0000, 0x0000,
  0x0400, 0x07C0, 0x0000, 0x07C0, 0x0100, 0x0380, 0x0000, 0x07E0,
  0x07C0, 0x1FF8, 0x1FF0, 0x3FEC, 0x3FF8, 0x7FDE, 0x3FF8, 0x7FBE,
  0x7FFC, 0xFF7F, 0x7EFC, 0xFFFF, 0x7FFC, 0xFFFF, 0x3FF8, 0x7FFE,
  0x3FF8, 0x7FFE, 0x1FF0, 0x3FFC, 0x07C0, 0x1FF8, 0x0000, 0x07E0,
  0x0000, 0x0000
};
```
- Copy the image into `AllocMem(sizeof(busy_image), MEMF_CHIP)` when the window opens and free it when the window closes.
- `busy_begin()`: `InitRequester(&block)` and `Request(&block, win)` (an empty requester blocks input), then `SetPointer(win, busy_sprite, 16, 16, -6, 0)`.
- `busy_end()`: `ClearPointer(win)` and `EndRequest(&block, win)`.

**Menus.** Use V33 `struct Menu`/`MenuItem`/`IntuiText` in topaz 8, attached with `SetMenuStrip` and removed with `ClearMenuStrip` before `CloseWindow`.
- Project: `About...` (COMMSEQ `?`) and `Quit` (COMMSEQ `Q`).
- Settings: `Dates YY-MM-DD` and `Dates YY-DD-MM`, each `CHECKIT` with `MutualExclude` of the other. Then `Sizes Full` and `Sizes Compact`, likewise.
- Set the initial `CHECKED` state from `state->prefs`. On `MENUPICK`, walk `ItemAddress`/`NextSelect`. A settings pick calls `amiga_ctl_set_prefs` and repaints the Browse rows.
- Item widths: `TextLength` of the longest label + `CHECKWIDTH` + `COMMWIDTH`.
- `About...` or the Help key: `AutoRequest` with `FujiNet Config`, `Hosts, catalogue and drives`, `for FujiNet NIO on the Amiga`, and an `OK` button.

**Keys.** Use `amiga_key_from_raw(msg->Code, msg->Qualifier)`.
- UP/DOWN/PAGE/TOP/BOTTOM move the current page's list (page = `layout.list_rows`; top = `-count`; bottom = `+count`).
- ACTIVATE triggers the page's primary action (Hosts→browse, Browse→open, Catalogue→`ActivateGadget(&edit, win, NULL)`, Drives→insert).
- PARENT→browse parent. CANCEL→quit. HELP→About. NEXT/PREV_PAGE→page±1 with wrap.

**Script mode.** This applies when `opts->script[0]`.
- After the first full paint, `fopen(opts->script, "r")` and `fopen(opts->result, "w")`. If either fails, show a fatal requester and return 20.
- For each `fgets` line: strip `\n`, write `"> " line`, call `amiga_script_line`, and fully repaint (the page may have changed).
- `WAIT` → `Delay(ticks)`. `QUIT` → stop.
- At the end, write `SCRIPT DONE ok=<n> err=<m>`, close both files, and return 0 if `m == 0`, else 5. The window closes afterwards.

**Exit.** CLOSEWINDOW, Project/Quit and Esc all quit immediately. Nothing is pending, because every edit has already been committed.

- [ ] **Step 1: Write the guest script the GUI must satisfy** (the failing acceptance input)

Create `tests/amiga/guest/accept.script`. It runs in the Amiberry harness (Task 15), whose FujiNet data root has `standard.adf` and catalogue slot 11 → `host:/standard.adf` (RO):
```text
; config-nio Workbench acceptance
page hosts
host add host:/
dump hosts
host select 3
browse
wait 25
select standard.adf
assign 30 ro
dump slot 30
page catalogue
wait 25
page drives
insert 30 DN0: ro
dump drives
wait 25
quit
```
And `tests/amiga/guest/eject.script`:
```text
page drives
eject DN0:
dump drives
quit
```

- [ ] **Step 2: Implement the GUI contract above in `amiga_gui.c`**

Keep functions small and name them after the contract headings: `gui_open`, `gui_close`, `gui_make_gadgets`, `gui_sync_gadgets`, `gui_paint`, `gui_paint_rows`, `gui_sync_editors`, `gui_do_action`, `gui_handle_key`, `gui_handle_menu`, `gui_run_script`, `busy_begin`, `busy_end`, `confirm`. A reviewer must be able to map each bullet to one function.

- [ ] **Step 3: Build both profiles and keep host tests green**

Run:
```sh
make amiga AMIGA_PROFILE=wb32 && make amiga AMIGA_PROFILE=wb13 && make test-amiga-host
```
Expected: no warnings from `src/platform/amiga/`, and `0 failures`.

- [ ] **Step 4: Interactive check on WB3.2** (evidence: screenshots of all four pages)

In `amiga-workbench --profile wb32-a1200`, check that you can:
- add a host, browse `fujinet.online`, and open a drawer with a double-click;
- go to the parent with Backspace, and assign a file to slot 30 RO;
- see it on the Catalogue page, then Insert it into DN0: on Drives and see `Slot 30 inserted in DN0:`;
- run `Dir DN0:` in a Shell, Eject, and confirm that `Dir DN0:` now fails;
- toggle Settings → Sizes Compact and see the browse sizes change;
- use Esc to quit.

- [ ] **Step 5: Commit**

```bash
git add src/platform/amiga/amiga_gui.c tests/amiga/guest
git commit -m "amiga: implement Intuition window, lists, gadgets, menus and script mode

Verified with: make amiga (wb32, wb13); make test-amiga-host;
manual WB3.2 walkthrough of all four pages (screenshots in task review)"
```

---

### Task 14: Workbench icon, documentation

**Files:**
- Create: `amiga/tools/mkinfo.py` (adapted from `~/dev/fujinet-iss-tracker/amiga/tools/mkinfo.py`, the user's own tool), `amiga/gfx/config-nio.icon.txt`, `amiga/icons/config-nio.info` (generated, checked in)
- Create: `docs/amiga-config.md`
- Modify: `Makefile` (`regen-amiga-icons` target), `README.md`, `RUNNING_TESTS.md`

- [ ] **Step 1: Add the icon art** `amiga/gfx/config-nio.icon.txt`. It is 40×20. Pens: `.` 0, `W` 1, `B` 2, `O` 3. The design is a floppy disk with an `FN` label and a network plug, and it reads correctly in both the 1.3 and 2.x+ palettes, following mkinfo's rules.
```text
........................................
..WWWWWWWBBBBBBBBBBBBBBBBWWWWWWWWW......
..W......BOOOOOOOOO...OOB.........B.....
..W......BOOOOOOOOO...OOB.........B.....
..W......BOOOOOOOOO...OOB.........B.....
..W......BOOOOOOOOO...OOB.........B.....
..W......BBBBBBBBBBBBBBBB.........B.....
..W...............................B.....
..W...............................BOOOO.
..W...WWWWWWWWWWWWWWWWWWWWWWWWW...BOOOOB
..W...W.......................W...BOOOOB
..W...W...OOOO..O...O.........W...BOOOO.
..W...W...O.....OO..O.........W...B.....
..W...W...OOO...O.O.O.........W...B.....
..W...W...O.....O..OO.........W...B.....
..W...W...O.....O...O.........W...B.....
..W...W.......................W...B.....
..W...WWWWWWWWWWWWWWWWWWWWWWWWW...B.....
..BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB.....
........................................
```

- [ ] **Step 2: Adapt mkinfo.py**

Copy the tool, then replace its `ICONS` table with a single entry:
```python
APP_TOOLTYPES = ["FMOUNT=SYS:C/fmount", "FUMOUNT=SYS:C/fumount"]
ICONS = [
    ("gfx/config-nio.icon.txt", "icons/config-nio.info", WBTOOL, None, 16384, APP_TOOLTYPES),
]
```
Keep its OS 1.x DiskObject format (two bitplanes, complemented highlight, no OS 2.x extensions) so every Workbench loads it. Add to the `Makefile`:
```make
regen-amiga-icons:
	cd amiga && python3 tools/mkinfo.py --preview ../build/amiga-icon-preview.png
```
Run: `make regen-amiga-icons && ls -l amiga/icons/config-nio.info`
Expected: the `.info` file is written, `check_icon` passes inside the tool, and the preview PNG shows both palettes. Inspect it.

- [ ] **Step 3: Write `docs/amiga-config.md`**

Sections:
1. What it does, with the feature map table from this plan's Design section.
2. Starting it: from Shell with arguments, or from Workbench with ToolTypes.
3. Using each page, with the keyboard and menu reference.
4. How Insert and Eject relate to `FMOUNT`/`FUMOUNT`/`FMOUNTRESTORE`, and the WB1.3 endpoint labels.
5. `SCRIPT=` language reference: the table from Task 11.
6. Developer notes: the module map, the fact that `make test-amiga-host` needs only gcc, and `make amiga AMIGA_PROFILE=wb13|wb31|wb32`.

In `README.md`, add a short "Amiga" section linking to it. State there that `amiga` is built on demand and is not part of `all-targets`. In `RUNNING_TESTS.md`, add a third test layer "Amiga host tests" with `make test-amiga-host [ONLY=name]` and the Amiberry node from Task 15.

- [ ] **Step 4: Commit**

```bash
git add amiga Makefile docs/amiga-config.md README.md RUNNING_TESTS.md
git commit -m "amiga: add Workbench tool icon with ToolTypes and user documentation

Verified with: make regen-amiga-icons (preview inspected in 1.3 and 2.x palettes)"
```

---

### Task 15: Workspace integration and guest acceptance (workspace repo)

**Owner:** workspace repo (`integration-tests/amiberry`, `scripts/amiga-artifacts`). Create branch `amiga-workbench-config` in the workspace before editing.

**Files:**
- Modify: `scripts/amiga-artifacts`. After the nio-apps block, build nio-config for **every** profile:
  ```sh
  make -C "$ROOT/repos/nio-config" amiga "AMIGA_PROFILE=$PROFILE" \
    "FUJINET_NIO_DRIVER_BUILD=$DRIVER_BUILD"
  ```
  Add `"$ROOT/repos/nio-config/build/amiga/$PROFILE/bin"` to the staging `for bin_dir` loop, and install `repos/nio-config/amiga/icons/config-nio.info` as `$STAGE/config-nio.info`.
- Modify: `integration-tests/amiberry/conftest.py`. Where `app_dir` is chosen (currently `"nio-apps" if case["project"] == "apps" else "nio-core-apps"`), add `"config"` → `repos/nio-config`. Keep the other two mappings unchanged.
- Modify: `integration-tests/amiberry/tests.toml`. Add two cases, `config-nio-wb32` (`environments = ["wb32"]`) and `config-nio-wb13` (`environments = ["wb13"]`, `amiga_artifact_profile = "wb13"`). Copy the `diskdevice-fmount` fields (`driver = true`, timeouts, `completion_mode = "nio_marker"`) and add `core_tools = ["fmount", "fumount"]`, `project = "config"`, `app = "config-nio"`.
- Create: `integration-tests/amiberry/startup/config-nio.sequence`, `startup/config-nio-wb13.sequence`, `integration-tests/amiberry/test_config_nio.py`
- Copy the two scripts from `repos/nio-config/tests/amiga/guest/` so the guest sees them. Either stage them next to the app, or `Echo` them from the sequence. Choose whichever mechanism the harness already uses to put files on `DH0:`.

- [ ] **Step 1: Write the failing guest test** `integration-tests/amiberry/test_config_nio.py`

```python
import pytest


@pytest.mark.parametrize("case", ["config-nio-wb32", "config-nio-wb13"])
def test_config_nio_gui_script_insert_and_eject(run_amiga_case, case):
    results = run_amiga_case(case)
    accept = results["config-nio-accept.result"]
    assert "HOST 3 host:/" in accept
    assert "SLOT 30 RO host:/standard.adf" in accept
    first_drive = "DRIVE DN0: 30 RO"
    assert first_drive in accept
    assert "SCRIPT DONE ok=" in accept and "err=0" in accept
    assert "KNOWN" in results["config-nio-type.result"]
    eject = results["config-nio-eject.result"]
    assert "DRIVE DN0: - -" in eject and "err=0" in eject
    assert "DIR RC=0" not in results["config-nio-after-eject.result"]
```

The `startup/config-nio.sequence` (WB3.2) follows the existing style:
```text
C:Assign T: RAM:
C:FailAt 21
C:fujinet-load-resident DEVS:fujinet-disk.device fujinet-disk.device
C:config-nio SCRIPT=DH0:accept.script RESULT=DH0:config-nio-accept.result
C:Type DN0:KNOWN.TXT >DH0:config-nio-type.result
C:config-nio SCRIPT=DH0:eject.script RESULT=DH0:config-nio-eject.result
C:Dir DN0: >DH0:config-nio-after-eject.result
C:Echo "DIR RC=$RC" >>DH0:config-nio-after-eject.result
C:FLS host:/amiga-e2e-complete/config-nio-wb32 >NIL:
```
The WB1.3 variant uses `startup_target = "S/StartupII"`, the same install/load steps as `diskdevice-wb13-*`, and `DN0:` (unit 0 is `DN0:` on 1.3 as well).

Run:
```sh
source scripts/env.sh && uv run --project integration-tests/amiberry pytest --run-amiga \
  --amiga-env wb32 --amiga-machine a1200-030 \
  "integration-tests/amiberry/test_config_nio.py::test_config_nio_gui_script_insert_and_eject[config-nio-wb32]"
```
Expected before the wiring: FAIL. The case is unknown, or the app was not built (`Amiga test application was not built`).

- [ ] **Step 2: Wire artifacts, conftest mapping, tests.toml, sequences** as listed under **Files**.

- [ ] **Step 3: Run the WB3.2 node**

Run the command from Step 1.
Expected: PASS. Keep the evidence directory (screenshots show the window on the Browse, Catalogue and Drives pages during the `wait` steps).

- [ ] **Step 4: Run the WB1.3 node**

Run:
```sh
source scripts/env.sh && uv run --project integration-tests/amiberry pytest --run-amiga \
  --amiga-env wb13 \
  "integration-tests/amiberry/test_config_nio.py::test_config_nio_gui_script_insert_and_eject[config-nio-wb13]"
```
Expected: PASS. If `Execute()` from the script-launched process fails on 1.3 because `C:Run` is missing, the transcript shows `ERR FMOUNT failed for DN0: (rc 0)`. In that case, add `C:Run` to the wb13 guest install, and document the requirement in `docs/amiga-config.md`. Do not weaken the assertion.

- [ ] **Step 5: Manual visual checklist** (record results and screenshots in the task review)

| Environment | How | Check |
| --- | --- | --- |
| WB3.2 A1200 | `amiga-workbench --profile wb32-a1200` | launch from the **icon** (ToolTypes honoured); 3D bevels use DrawInfo pens; menus with checkmarks; busy pointer during Browse |
| WB3.1 with a 13-point screen font | set Font prefs, relaunch | layout fits, string gadgets are tall enough, and no text overlaps |
| KS1.3 / WB1.3 | wb13 environment | launch from the **icon** as well as the Shell; flat 1.3 pens (white/black/orange); Insert of `HN0:` with an HD ADF |
| KS2.04 (real-hardware proxy, per the A500+ note) | Amiga Forever `amiga-os-204.rom` with a small xdftool floppy whose startup runs `config-nio` | title bar and window chrome draw with no stray lines; layout read from real borders |

- [ ] **Step 6: Commit (workspace repo)**

```bash
git add scripts/amiga-artifacts integration-tests/amiberry
git commit -m "amiga: stage config-nio and add Workbench GUI acceptance cases

Verified with: pytest --run-amiga test_config_nio.py[config-nio-wb32] and
[config-nio-wb13]; manual WB3.2/WB3.1-bigfont/WB1.3/KS2.04 checklist"
```

---

### Task 16: Final regression gate

**Files:** none

- [ ] **Step 1: Existing targets unchanged**

Run (from `repos/nio-config`):
```sh
source ../../scripts/env.sh
make TARGET=linux FUJINET_NIO_LIB=../fujinet-nio-lib && cmp build/linux/bin/config-nio ../../build/nio-config-baseline/config-nio.linux && echo linux-identical
make TARGET=bbc FUJINET_NIO_LIB=../fujinet-nio-lib && cmp build/bbc/bin/config-nio ../../build/nio-config-baseline/config-nio.bbc && echo bbc-identical
git diff master --stat -- src/platform/bbc src/platform/msdos src/platform/portable src/platform/linux bbc msdos integration-tests/beebium src/main.c
```
Expected: `linux-identical`, `bbc-identical`, and an **empty** diff stat. If Watcom is available (`source ~/.local/bin/add_watcom.sh`), also run `make TARGET=msdos FUJINET_NIO_LIB=../fujinet-nio-lib` and expect success. If it is not available, record that in the review.

- [ ] **Step 2: Amiga gates**

Run: `make test-amiga-host && make amiga AMIGA_PROFILE=wb32 && make amiga AMIGA_PROFILE=wb13`
Expected: `0 failures`, and both builds succeed.

- [ ] **Step 3: Request the whole-branch review** with superpowers:requesting-code-review. Then finish with superpowers:finishing-a-development-branch. Do not push.

---

## Out of scope (follow-ups, not part of this plan)

- Adding `config-nio` to `configs/amiga/release-adf-*.yaml` (the release ADF contents).
- A localised catalog (`locale.library`, V38+) and a font-sensitive layout that uses the screen font instead of topaz 8.
- HDF/RDB media on the Drives page (tracked separately in `backlog/amiga-hdf-rdb-support.md`).
