# config-nio state models

> **Terminology note:** “embedded” and “external” are historical names for
> where the tables live relative to the C state structure. They do **not** mean
> embedded RAM versus external RAM, nor persistent storage versus volatile
> storage. Both models use volatile working memory; persistence is handled
> separately by the app-store layer.

`config_nio_state_t` is the working state passed through the config-nio
application. It is not the persistent configuration format. Hosts, mappings,
preferences, and slot catalog entries are persisted through the `config-nio`
app-store namespace (and, for the BBC implementation, the slot/catalog cache
is only a display cache).

The state model is selected at compile time in `include/config_nio.h`:

```c
#ifdef CONFIG_NIO_EXTERNAL_TABLE_STATE
#include "config_nio_state_external.h"
#else
#include "config_nio_state_embedded.h"
#endif
```

## “Embedded” state: tables inside the state object

The embedded model puts the working tables directly inside
`config_nio_state_t`:

- the host URI array;
- the visible slot records;
- the drive mapping array;
- the directory-entry array and its metadata;
- preferences, browse path, and a 96-byte status string.

The ordinary table functions in `src/platform/portable/config_nio_tables.c`
read and write those fields directly. This is the simplest model and is used
by the portable implementation and by the MS-DOS and Linux targets. The
Atari reads and writes the same records through its own compact core
(see [atari-config.md](atari-config.md)).

The limits depend on the compiler family:

| Target family | Hosts | Entries | URI limit | Entry limit | Notes |
| --- | ---: | ---: | ---: | ---: | --- |
| MS-DOS/Linux | 8 | 20 | `FNSVC_MAX_URI` (255) | 79-character names | Includes size and modification time |

The embedded structure is convenient, but its size grows as the table limits
grow. On a small 6502 target, large arrays inside the state object also make
the C compiler's stack and temporary workspace harder to place safely.

## “External” state: tables outside the state object

The external model keeps only control fields and transient UI data in
`config_nio_state_t`:

- host count;
- slot page position/count/more flag;
- entry count and pagination metadata;
- browse path;
- an unused preferences placeholder;
- no host, slot, mapping, or directory-entry arrays.

The accessor names remain the same, but the macros in
`config_nio_state_external.h` ignore the `state` argument and route operations
to platform storage. This preserves one common application API while allowing
the storage implementation to be written in assembly or placed outside the
normal C state object.

In clearer terminology, these could be called **inline-table state** and
**external-table state**, or **state-in-struct** and **platform-table state**.
The latter is the most precise description of the current implementation:
the table bytes are owned by platform-specific storage/accessors, while the
application state object carries only pagination and transient fields.

### BBC

BBC and Master builds enable `CONFIG_NIO_EXTERNAL_TABLE_STATE` in
`makefiles/build.mk`.

For an unexpanded BBC, `src/platform/bbc/config_nio_tables.s` owns the tables
in dedicated BSS regions. The current layout is approximately:

- 16 host roots, capped at 96 characters plus terminator;
- 8 visible slot records of 30 bytes each;
- 8 three-byte drive mappings;
- 12 directory entries of 32 bytes each.

The host table's 96-character root cap is an implementation/memory limit; it
is distinct from the public `CONFIG_NIO_URI_MAX` value of 128. Composed browse
URIs use separate workspace buffers. Slot catalog persistence is still in the
app store; the RAM slot table is only the current/previous page cache.

For the Master/XRAM profile, the same assembly accessors address fixed XRAM
regions instead of consuming the corresponding tables in the normal BBC RAM
area. This is the main reason for the external model: it gives the transient
BBC program a small, stable state object and lets the table implementation
choose normal RAM, XRAM, or another backing area.

`config_nio_load()` still clears the state and loads persistent hosts and
mappings. It also invalidates the BBC slot-page cache before the first lookup;
external state does not mean that persistence is bypassed.

### Master

Master uses the same external header and accessor API as BBC, but enables the
XRAM table profile. The larger logical limits can therefore be supported
without placing all table bytes below the BBC program's normal high-memory
boundary.

## Status messages

The embedded header declares:

```c
#define CONFIG_NIO_STATUS_MAX 95
char status[CONFIG_NIO_STATUS_MAX + 1];
```

`src/platform/portable/config_nio_state.c::config_nio_set_status()` copies a
message into that buffer, truncating it to fit. The portable UI renders
`state->status` on each screen. This is the meaningful status path used by the
portable implementation and is also available to the embedded
MS-DOS and Linux builds where the embedded structure is selected.

The external header deliberately defines:

```c
#define CONFIG_NIO_STATUS_MAX 0
#define config_nio_set_status(state, msg) do { (void) (state); } while (0)
```

Consequently status calls compile to no operation for BBC/Master external
state. They are retained at call sites for source/API compatibility, but no
message is stored and no external status buffer exists. The BBC UI has its own
screen/status handling and many BBC errors are reported directly by the
command or immediate UI path; however, any code that relies solely on
`config_nio_set_status()` produces no visible message on the external build.
This is a real semantic difference, not merely a memory optimization.

The zero-length `status` member still contributes a terminator byte to the
external structure because it is declared as `status[CONFIG_NIO_STATUS_MAX +
1]`; the significant saving comes from removing the large embedded arrays,
not from removing this one byte.

## Choosing a model for BBC memory work

Changing BBC from external to embedded state would put hosts, slots, mappings,
entries, preferences, browse path, and status into the transient C state
object. It would also switch the accessors away from the assembly table/XRAM
implementation. That is not a simple reduction: an embedded BBC build would
have much smaller logical limits and would duplicate storage that the current
assembly implementation already manages.

The external model does not make the tables disappear. On an unexpanded BBC,
the external tables still consume dedicated BSS, so total RAM must be assessed
as:

```text
external state object + external table BSS + C/assembly workspace
```

Its benefit is separation and placement flexibility: table bytes can be
optimized independently, moved to XRAM on Master, and kept out of the C
structure/ABI. For further BBC savings, first inspect the table constants and
whether a table can move to XRAM or a smaller display/cache representation can
be used. Do not assume that replacing external state with embedded state will
save memory.
