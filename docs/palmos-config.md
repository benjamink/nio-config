# Palm OS NIO Config

**NIO Config** provides native 160×160 **Hosts | Info | Setup** pages for
Palm OS 3.1/3.5. It uses the installed NioLib shared library for every remote
operation and for local transport preferences. It embeds no NIO packet core,
physical backend, or separate transport preferences.

The application database remains **NIO Config / appl / NiC1**, distinct from
NioLib's NiL1 and NioLibTest's NiT1. This is a project identity choice, not a
claim of a globally registered Palm creator ID.

## Dependencies and build

Use matching public headers and library artifacts from `fujinet-nio-lib`.
Hosts, installation, Info and Wi-Fi require ABI major 1, minor >=6 and
`nioLibCapServices`. Setup transport settings and Clock Test remain available
with ABI 1.2 or later and Transports, Clock and Client capabilities. USB also
requires `nioLibCapUSB`. Unsupported services are rejected before new traps
are invoked. The source includes `NioLibServices.h` through `NioLib.h`.

Build the installed library and client archive first, with the same SDK;
see `fujinet-nio-lib/docs/palmos-shared-library.md`. For the installed
[palmdev-toolchain](https://github.com/mozzwald/palmdev-toolchain):

```sh
export PALMOS_BUILDER=container
export PALMDEV_TOOLCHAIN=../palmdev-toolchain
export PALMOS_SDK_VERSION=3.5
make -C ../fujinet-nio-lib palmos-shared
make palmos FUJINET_NIO_LIB=../fujinet-nio-lib
```

Resolve `PALMDEV_TOOLCHAIN` appropriately when building from another directory,
or use an absolute path. No toolchain or SDK is installed automatically.
`PALMOS_BUILDER=auto|native|container` selects the existing library launcher;
container explicitly selects palmdev-toolchain. SDK 3.1 is also buildable:

```sh
make -C ../fujinet-nio-lib palmos-shared PALMOS_SDK_VERSION=3.1
PALMOS_SDK_VERSION=3.1 PALMOS_BUILD=build/palmos-3.1 make palmos
```

`NIOLIB_BUILD` defaults to
`$(FUJINET_NIO_LIB)/build/palmos-shared-$(PALMOS_SDK_VERSION)`.
The default app output is `build/palmos-usb/NioConfig.prc`, with map, symbols
and disassembly beside it. Despite that legacy directory name, this single
application selects Springboard, serial or USB at runtime. The build packages
an explicit resource list so stale forms/alerts cannot enter a new PRC.
Other platform source lists and builds are unchanged.

## Hosts and browsing

The application opens Hosts immediately without starting a transport or
fetching data. Tap **Refresh** to load the shared list when NIO is running;
Setup remains reachable even with a missing or misconfigured host. Opening
Hosts again in the same run retains the loaded list without another fetch.
Explicit remote operations still use NioLib's synchronous service timeouts.

Refresh reads the same remote AppStore record as the other platforms:
namespace **config-nio**, key **hosts**, newline-separated text. There are up
to eight hosts, each up to 240 bytes on Palm. A missing record displays
`sd0:/`, `fujinet.diller.org` and `fujinet.online`; these defaults are written
only after an explicit edit. An existing empty record stays empty. An
oversized or malformed record is rejected, never silently shortened and saved.

- **Add/Edit** accepts a bare TNFS hostname, `host:port`, or a URI such as
  `tnfs://host/PALM/`, `sd0:/` or `persist:///`. Save a blank edit to remove it.
- **Browse** opens the selected host. Folder rows begin with `/`; **Open**
  enters a folder or offers to install a Palm file. **Up** goes to its parent.
- Both lists have native vertical scrollbars and **< / >** buttons. Hosts
  scrolls its seven visible rows locally. The file scrollbar moves in windows
  of eight entries; it fetches only when the pen is released, not while
  dragging the thumb. This follows the Palm SDK's
  [non-dynamic scrolling event contract](https://users.fuw.edu.pl/~michalj/palmos/EventRef.html).
- In the file browser, **< / >** fetch preceding/following windows of eight entries. The counter
  shows the current range and directory total; directories are not capped at
  100 entries. Long names may require several packets for one window.
- **R** refreshes the remote directory cache and returns to its first window.
  **?** shows the current folder URI. The bottom **Hosts** button returns to
  the host list; the top Hosts tab preserves the current browsing context.
- **Refresh** on the host list reloads the shared record, including changes
  from another platform or a different selected backend.

Single taps select a host or file-list entry. Double-tap the same entry to run
its usual **Browse/Open** action: browse a host, enter a directory, or open the
normal file-install confirmation. Refreshing, scrolling or changing pages
clears the previous tap. Palm OS 3.5 uses its native pen tap count and waits for
the list selection after pen release before opening another form/dialog
([Palm OS list event flow](https://palm.wiki/development/docs/601/PalmOSCompanion/UserInterface.html)).
SDK 3.1 builds recognize the same selected row within half a second instead.

The browser uses `NioLibFileListPages`, with sorting and folders first. It
validates the returned page boundaries and basenames before publishing rows.
Names cut by the server are displayed but cannot be opened. The scratch
capacity comes from `NioLibGetTransportStatus.maxPacket`, less framing and
bounded by the Palm buffer; BeginClient's capacity is a session count.

Host writes can span packets. AppStore does not provide a transaction here;
a failed write is reported and the record must be reloaded before further
editing. It may contain only the completed portion of that write.

## Installing a Palm database

**Open** on a `.prc`, `.pdb` or `.pqa` asks before installing. FileStat obtains
the size, FileRead streams bounded chunks into Palm OS `ExgDBRead`, and the
app notifies an installed application with `sysAppLaunchCmdSyncNotify`.
During the transfer an **Installing** popup shows the filename, `<progress> of
<total> bytes`, and a meter that fills from left to right. It starts at zero
and closes before the completion or error message, restoring the browser.
Data chunks are up to 256 bytes, using 512 bytes of service scratch. FileRead
needs a ten-byte service header in addition to its data. The transport packet
limit, including framing, can reduce the chunk size. Larger 512-byte chunks
regressed the first PRC read on Springboard and require further library/bridge
validation before enabling. Progress redraws and auto-off resets are
limited to once per second while data arrives, plus the first chunk and
completion; the final byte count is always shown.
Replacement requires confirmation. NIO Config refuses to replace itself while
running and reports databases that cannot be deleted. OS installation errors
and transport/service failures are reported separately. Read failures include
the failing stage and byte position; link cleanup failure is distinguished
from database installation failure. The stat size bounds the stream: no
read is sent at/after the known EOF, including full final packets whose flags
do not include EOF. Read-only I/O errors may be retried up to three times at
the same offset, only while passive transport status reports no recovery
requirement. Palm errors, timeouts and writes are not automatically retried.
A requested reset is reported after installation.

This workflow is adapted from GPL-3.0
`fujinet-palm/apps/FujiConfig/FujiConfig.c`. Its custom `filedev.c` codec and
static transport library are replaced by installed NioLib services. HotSync
queueing and disk image mounting are not included.

## Info

Info reads firmware version, build profile, configured SSID, live Wi-Fi state
and RSSI, IP address, gateway, DNS and station MAC. It also shows the NioLib ABI,
selected transport and packet size. Optional unsupported metadata appears as
unavailable. Other service/transport errors stop the batch, identify the
failed command and retain its error instead of silently marking data missing.
**Refresh** rereads it; local availability alone is not proof of
an Internet connection.

## Setup

The transport row directly below the page tabs is **Springboard | USB | Serial**.
Only Serial shows the **Baud** and **Flow** popup selectors below it, with
9600/19200/38400/57600/115200 baud and None or RTS/CTS (8N1). Switching transport
preserves the serial values but hides those controls for USB and Springboard.
The bottom row is **WiFi | Save | Test | Reset**; the area above it shows the
latest message/test result and any Palm/NIO error codes.
**Save** writes preferences through NioLib and verifies readback. **Test**
requests Clock time over the saved transport and releases its session.
Test is blocked while cleanup of a previous client is incomplete.
The former Status button only inspected local backend state; it did not test
connectivity and has been removed. Local status is still checked internally for
startup, saved settings, Test and recovery; **Test** performs the connection check.
Unsaved/invalid transport changes must be saved before remote operations;
exiting does not implicitly save them. Missing or unavailable transports never
silently fall back.

**Reset** is a link-recovery action, not a Palm reset. It requires NioLib
ABI 1.5 or later and `nioLibCapRecovery`; older libraries are never called
through the new trap. Use it only when the result area reports **Link recovery required**.
Save the intended transport first and end active NIO clients. The confirmation
explains which peer must be retired; **Cancel** is the default.

For USB, return Home, stop the NIO host and physically disconnect/reconnect the
cradle/device to retire the old USB session. Then open Setup, select USB and
**Save**, tap **Reset**, and confirm only after doing those steps. Start
the host and arrange interface access before **Test**. For serial, retire the
remote peer's sessions and buffered replies first; Springboard requires its
link and host to be reinitialized. Reset clears only the selected transport's
marker through NioLib. It sends no network request, cannot undo a previously
executed command and does not retry one. Active clients, retained driver
resources or failed preference verification leave recovery blocked. See
`fujinet-nio-lib/docs/palmos-shared-library.md` for the recovery contract.

**WiFi** queries NIO for its configured SSID and live connection state when
opened, prepopulating the SSID field. NIO returns only whether a password is
stored, not the password itself: the password field starts blank, with the
status noting when a saved key is not shown. Enter the password to join a
secured network. No scan is started automatically.
**Scan**, **Join** and **Done** sit along the bottom below the scan results.
The page accepts an SSID or a selection from **Scan**, plus a password
(blank for an open network). The field is visibly labeled **Password (shown)**;
its text is erased after submission and on leaving the page, and is never
stored by this app. Scan displays up to eight returned networks; an unlisted
SSID can be typed. **Join** uses firmware's trial join, as on the compact
platform: credentials are saved after connection succeeds; failure restores
the previous network. The event loop polls once per second, up to 60 seconds,
without holding a session between polls. Transport changes wait for the trial
result. An accepted trial remains pending if session cleanup or status polling
fails; errors are reported without resubmitting credentials. Polling continues
until a terminal result or the 60-second limit. Leaving the application stops
polling; firmware completes its trial independently. Older/unsupported firmware
reports the service error.

Setup transport inspection and saving send no network traffic. Hosts, Info,
Wi-Fi and Test use synchronous calls and can wait for NioLib timeouts. Every
operation ends its foreground session; library references close on normal exit.
Failed cleanup never forces removal of an owned library or bypasses recovery.
USB requires a compatible HsUs Generic Port 2 host and a released HotSync cradle;
see `fujinet-nio/docs/visor-usb-host.md`.

## Tests and qualification

See [RUNNING_TESTS.md](../RUNNING_TESTS.md#palm-shared-library-configuration).
The compiled m68k suite runs the actual app and installed library with mocked
Palm OS traps and remote wire peers. It checks capability gates, transport
save/readback and cleanup, shared host records, pagination beyond 100 files on
all three transports, long/cut names, malformed replies, streamed install reads,
Wi-Fi trial submission/polling, offline Hosts startup, scrollbar bounds and
release-only fetching, exact EOF, bounded read retries and error reporting. PRC resource checks exclude embedded NIO
services, local preferences and physical backends.
UI checks cover serial-only popup visibility and selections, the bottom action
rows, Wi-Fi settings/status on entry without an automatic scan, scan selection,
and double-tap host/directory/file actions with stale/different-row protection.
Cleanup failure checks preserve a retained client token across Test and keep
accepted Wi-Fi trials tracked through cleanup or polling errors without
resubmitting credentials or allowing a transport change before a terminal
result or timeout.
Recovery tests execute the real library trap and check confirmation/cancellation,
older ABI/capability gates, active-owner and retained-driver refusal, failed
marker writes, transport isolation and a subsequent mocked USB Clock request.
They do not establish that a physical USB reconnect happened.

The compiled tests and SDK 3.1/3.5 builds do not establish complete device
acceptance. On a Visor Prism using Springboard, the user reported working TNFS
PRC installation with the conservative 256-byte reads after updating NioLib.
The revised Setup/Wi-Fi layouts, double-tap browser and installation progress
popup have also received positive device feedback; this is not exhaustive
coverage of every control or failure path.

Saved Springboard settings and repeat-launch Clock tests were reported passing.
The confirmed Setup Reset action was reported working for USB on the Prism.
These results do not qualify serial or Springboard recovery, or arbitrary
physical failure modes. Physical Wi-Fi joining, serial hardware, macOS runtime
and long-duration heap/stress use still need focused acceptance testing.
No background networking, automatic transport switching or HotSync queueing
is implemented.
