# Vivado driver coexistence: development scope

Status: design only; no native-driver backend is implemented yet.

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
