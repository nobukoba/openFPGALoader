# Vivado driver coexistence: development scope

Status: experimental implementation with mock tests. Real Adept Runtime,
Windows driver coexistence and physical HS2/HS3 operation remain unverified.

## Initial implementation

The optional `ENABLE_DIGILENT_ADEPT` backend uses the native Adept DMGR and
DJTG APIs, selected with `-c digilent_adept`. The original `digilent_hs2`
and `digilent_hs3` cable names still select libftdi. The new backend is
generic Adept USB JTAG; it does not claim that every Adept device has been
validated. HS2 and HS3 are the first hardware validation targets.

Implemented: USB/JTAG capability filtering, exact serial selection,
zero-based cable index, explicit Adept device selector, speed negotiation,
TMS and TDI/TDO transfers, final-bit TMS exit, simultaneous TMS/TDI transfers,
clock generation, transfer timeout and handle cleanup. Multiple matching
cables require an explicit selector. SDK failures throw and invalidate the
connection; no automatic retry is attempted after the TAP state is uncertain.
The SDK controls device initialization, rather than reusing FTDI GPIO setup.

The first revision links to separately installed SDK/runtime libraries.
Runtime loading remains a possible follow-up. Vendor headers and binaries
are not bundled. The minimal test API is a fake and cannot access hardware.

## Build and first hardware checks

Install the official Digilent Adept SDK and Runtime for the target architecture.
Keep the existing Vivado-compatible cable driver. On Windows, use the MinGW/MSYS2
build environment supported by openFPGALoader and provide SDK include and
library locations. SDK and Runtime may be installed in separate locations;
all three CMake variables can be set explicitly:

```sh
cmake -S . -B build-adept -DENABLE_DIGILENT_ADEPT=ON \
  -DADEPT_INCLUDE_DIR=/path/to/sdk/inc \
  -DADEPT_DMGR_LIBRARY=/path/to/target/library/dmgr.lib \
  -DADEPT_DJTG_LIBRARY=/path/to/target/library/djtg.lib
cmake --build build-adept --parallel
```

Use the actual target-compatible import/shared libraries (for example `.lib`,
`.dll.a` or `.so`), not the placeholder paths above. Alternatively set
`ADEPT_ROOT` to an installation prefix containing `inc`/`include` and `lib`/`lib64`.
Make the vendor Runtime DLLs and their dependencies available through their
normal installed loader path. A real vendor-linked Windows binary has not yet
been built or tested in this environment.

Release the cable from Vivado/hw_server, then start with IDCODE detection:

```sh
openFPGALoader -c digilent_adept --usb-serial-num ACTUAL_SERIAL --detect
```

For a single connected USB JTAG device, omit the serial selector. For an Adept
alias/connection selector, use `-d`; use only one of `-d`, `--usb-serial-num`
and `--cable-index`. Index order is enumeration order and may change; serial
selection is preferable for repeatable multi-cable use.

After IDCODE detection is validated on a test target:

```sh
# SRAM load (not yet hardware-validated for this backend)
openFPGALoader -c digilent_adept --usb-serial-num ACTUAL_SERIAL design.bit
```

Direct FTDI SPI, FTDI GPIO/status/channel options, USB VID/PID/bus overrides
and inverted read edge are rejected. JTAG flash programming still requires the
existing target-specific bridge/flash support and separate hardware validation.
`--scan-usb` remains the existing libusb scanner, not an Adept enumeration command.
All new SDK transfers are synchronous, so backend `flush()` has no queued work.
Chunks contain at most 8192 bits and at most one second of clocks at the
negotiated speed; the SDK transfer timeout is set to 5000 ms. Enumeration/open
timing is controlled by the vendor runtime, not bounded by this transfer timeout.

## Tests

No SDK installation or hardware is required for the fake API tests:

```sh
cmake -S tests/adept -B build-adept-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build-adept-tests --parallel
ctest --test-dir build-adept-tests --output-on-failure
```

Tests cover bit order, 0/1-bit and non-byte-aligned lengths, 8192-bit chunk
boundaries, low-frequency unaligned chunks, final TMS exit with TDO capture,
in-place buffers, device selection, SDK failures, no retry after transfer
failure, negotiated speed and cleanup. Integration tests run the actual `Jtag`
class through a simulated Kintex-7 IDCODE chain and a detection failure.
The dedicated workflow runs mock tests on Linux and native Windows and compiles
the Linux Xilinx CLI against the fake SDK. These are not vendor driver tests.

The backend translation unit was also compiled with `-Wall -Wextra -Werror`
against Digilent-authored Adept SDK 2.0.1 headers (`dpcdecl.h`, `dmgr.h`, `djtg.h`)
retrieved from the archived SDK at
https://github.com/AlexRFox/soc/tree/master/digilent.adept.sdk_2.0.1/inc .
The Digilent-authored `DjtgDemo` sample in that SDK establishes the packed
TDI/TMS pair order. This checks declarations against an archived SDK, not ABI
compatibility with a current Windows runtime. Check against the installed
current SDK before a hardware release.

Local validation of this revision (Linux): both CTest targets passed, the
Xilinx CLI built with the mock SDK and with Adept disabled, and SDK-missing /
unsupported-option errors were checked. AddressSanitizer and UndefinedBehaviorSanitizer
tests passed with leak detection disabled because this execution environment
does not expose the process information LeakSanitizer needs. Mock counters
verify balanced device/JTAG acquisition and release on tested failure paths.
Windows CI and vendor-runtime/hardware results must be recorded separately;
the existence of a workflow is not evidence that it has passed.

## Goal

Use Digilent JTAG-HS2 and JTAG-HS3 on Windows without replacing the cable drivers used by Vivado. Coexistence means sequential access with the same installed drivers, not simultaneous ownership of one cable.

## Backend decision

Evaluate Digilent Adept SDK (device enumeration and JTAG API) first for HS2/HS3. Evaluate FTDI D2XX separately: D2XX availability alone does not establish that a particular Digilent cable can be opened correctly with its installed driver and identifiers. Do not claim Vivado internally uses one particular API without evidence.

Keep vendor SDK binaries external until their redistribution terms are checked. An optional runtime-loaded backend can allow the existing libftdi build to remain usable without the SDK.

## Source findings

- src/ftdipp_mpsse.hpp exposes ftdi_context to derived classes.
- src/ftdipp_mpsse.cpp handles enumeration, libusb descriptors, USB open/close, MPSSE configuration, GPIO and buffered transfer in one class.
- src/ftdiJtagMPSSE.cpp also calls ftdi_read_data directly, so changing only the base class would leave libftdi-dependent reads.
- A Digilent JTAG backend should integrate through the existing JTAG interface rather than pretend that the Adept JTAG API is a libftdi byte transport.

## Implementation sequence

1. Confirm official Adept SDK functions, installed runtime behavior and cable enumeration on Windows.
2. Add optional SDK detection/runtime loading and a separate Digilent JTAG implementation behind explicit cable/backend selection.
3. Implement serial-number selection, clock setting, TAP transitions and bit transfers; bound waits and handle short transfers, unplug and busy-device errors.
4. Add mock-based JTAG packing/error tests and Windows build checks; preserve existing libftdi operation.
5. Validate HS2 and HS3 against real hardware before marking the backend supported.

## Acceptance checks

- Enumerate HS2 and HS3 with the original Vivado-compatible driver installed; no Zadig replacement.
- Select either of two attached cables by serial number and read the expected JTAG IDCODE.
- Program SRAM and verify an observable target result.
- Program flash only on a designated test target, then verify readback and power-cycle boot.
- Run Vivado, close/release its cable connection, run openFPGALoader, release it, and reconnect Vivado without driver changes.
- A busy cable produces an actionable error without disrupting the other process.
- Repeated connect/disconnect and unplug during transfer finish with bounded errors and no leaked handle.

## Validation boundary

This development environment does not provide a Windows host, vendor runtime or attached HS2/HS3/FPGA. Build and mock tests cannot substitute for those hardware acceptance checks.
