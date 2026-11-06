# Palm OS NIO Config

**NIO Config** is a native Palm OS 3.5 transport selector for the installed
NioLib shared library. It uses standard 160×160 forms on color or monochrome
screens. It configures the Palm's local connection, not server host/device slots.
It contains no NIO packet core, physical transport or independent preferences.

The application database is **NIO Config / appl / NiC1**. NiC1 is distinct from
NioLib's NiL1 and NioLibTest's NiT1; this is a project identity choice, not a
claim of a globally registered Palm creator ID.

## Dependency and build

Build the installed NioLib and matching client archive in `fujinet-nio-lib`
first; see that repository's `docs/palmos-shared-library.md`.
Current library ABI is **1.4**. NIO Config accepts ABI major 1, minor >=2 with
Transports, Clock and Client capabilities. USB is enabled only when the installed
library reports `nioLibCapUSB=0x20` (provided since ABI 1.3). Older compatible
libraries still support CS1/serial settings; USB selection is rejected clearly.
Headers come from that repository, never vendored here.

With prc-tools/SDK 3.5, PilRC and build-prc on PATH, from this repository:

```sh
make palmos FUJINET_NIO_LIB=../fujinet-nio-lib \
  NIOLIB_BUILD=../fujinet-nio-lib/build/palmos-shared-usb \
  PALMOS_BUILD=build/palmos-usb
```

Or from the parent of sibling checkouts, after building NioLib:

```sh
docker run --rm --network none --user "$(id -u):$(id -g)" \
  -v "$PWD:/work" -w /work/nio-config fujinet-palm-springboard-tools \
  make palmos FUJINET_NIO_LIB=../fujinet-nio-lib
```

Output is `build/palmos-usb/NioConfig.prc`, with symbols/map/disassembly beside
it. Use NIOLIB_BUILD to select a non-default library build. Existing platform
builds and generated BBC assets are unaffected by this opt-in target.

## Install and configure

Install NioLib and NioConfig into internal Palm RAM through ordinary HotSync.
Before upgrading NioLib, end dependent apps, return Home and verify safe library
release as described in the library guide. No Springboard ROM/firmware rebuild
is required. NioConfig can read/save settings without a physical module or peer.

1. Open NIO Config and choose **Springboard**, **Serial** or **USB**.
2. For Serial, cycle baud through 9600/19200/38400/57600/115200 and choose no
   flow control or RTS/CTS. Communication is 8N1; default baud is 115200.
3. Tap **Save**. The app reads the settings back and verifies them.
4. Optionally tap **Test**, which opens the saved transport, requests Clock time
   and ends its foreground session. A successful response says `Clock reply: PASS`.
5. Return Home before opening ISS, Weather or another NioLib client.

USB means Handspring HsUs Generic Port 2 and requires a compatible NIO USB Host,
not the RP2350 Springboard link. Springboard uses the existing CS1 module and its
separate host link; Serial requires a physical serial cable/cradle and SLIP peer.
The USB cradle used for HotSync must be released by the HotSync process before
the libusb host can claim it. Host build/ownership guidance is in
`fujinet-nio/docs/visor-usb-host.md`.

Settings are saved by NioLib, persist across launches/resets and take effect at
the next client session. An active owner makes Save return BUSY. Missing settings
display the Springboard default; malformed settings require explicit repair/Save.
Unavailable transports never silently fall back. Baud/flow edits are active only
for Serial. Unsaved changes must be saved before Test; exiting does not save them.

Configuration inspection/saving sends no network traffic. Passive availability
is not proof of a host or Internet connection. Test is synchronous, can wait for
the library's operation timeout, and displays separate Palm/NIO errors. It releases
foreground ownership and closes its library reference on normal exit. A retained
driver-resource hold is left to the library's safe later cleanup; the app never
forces removal or clears ambiguity markers to bypass recovery.

## Tests and qualification

See [RUNNING_TESTS.md](../RUNNING_TESTS.md#palm-shared-library-configuration)
for compiled m68k tests with mocked Palm OS/transport peers. Coverage includes
ABI/capability checks, missing library, old-library USB rejection, configuration
validation/save/readback, absent link, Clock, repeated launch/cleanup and failed
USB Close followed by later recovery. Linked symbols exclude embedded NIO services,
preferences and physical backends. These are compiled-code mocks, not Palm OS.

On a Visor Prism, saved Springboard settings and repeat-launch Clock tests passed.
Linux USB selection passed NioLibTest Clock + two HTTP and ISS → Weather → ISS;
switching back to CS1 passed Clock + two HTTP. Post-audit teardown/readiness/host
fixes also passed the minimal USB/CS1 regression without changing this app.
The later JSON app/library update was separately reported working and faster;
NIO Config needed no change. No benchmark is claimed.

Serial hardware, macOS compile/runtime, long-duration heap/stress use and arbitrary
physical error recovery remain unqualified. ESP32-S3 USB Host is not implemented.
This app does not implement Hosts/Devices, application installation, background
networking, HotSync or automatic transport switching.
