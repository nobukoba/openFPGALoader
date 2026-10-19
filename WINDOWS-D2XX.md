# Windows FT232H / HS3 with the existing FTDI driver

This fork adds `USE_FTDI_D2XX=ON`. It uses the installed Windows x64 FTDI CDM driver through the system `ftd2xx.dll`, without installing, replacing, or restoring USB drivers. [日本語](WINDOWS-D2XX.ja.md)

Build in an MSYS2 UCRT64 shell after installing CMake, GCC, pkgconf, libftdi, libusb and zlib development packages:

```sh
cmake -S . -B build-d2xx -G Ninja -DUSE_FTDI_D2XX=ON -DENABLE_CABLE_ALL=OFF -DENABLE_FTDI_BASED_CABLE=ON -DENABLE_VENDORS_ALL=OFF -DENABLE_XILINX_SUPPORT=ON -DENABLE_UDEV=OFF -DENABLE_USB_SCAN=OFF
cmake --build build-d2xx
./build-d2xx/openFPGALoader.exe --scan-usb
```

Close Vivado Hardware Manager / hw_server before accessing its cable. Scan is read-only and uses D2XX enumeration. FT232H USB identity alone does not establish that the cable is an HS3; confirm the physical cable before choosing `digilent_hs3`.

This opt-in backend currently supports FT232H interface A (0403:6014), including HS3. It requires exactly one matching device. With multiple devices, supply the intended `--usb-serial-num SERIAL`; duplicate matches stop. libusb bus/address and device-index selection are rejected. If a cable is currently using WinUSB, this backend cannot use it until the user restores the FTDI driver separately. It never changes the driver itself.

Xilinx Platform Cable USB (03fd) uses a different vendor driver/API and is **not supported by this backend**. Other FTDI chip families and character-based transports are also outside this implementation. Normal Linux/macOS builds retain libftdi; Windows is required for this opt-in option.

The DLL is loaded from System32 only. FTDI headers/constants and ordinary non-D2XX build dependencies still apply; the FTDI DLL/driver is not redistributed. Reads have a 3-second driver timeout; incomplete transfers stop without automatic programming retries. Persistent FPGA flash operations still use the existing openFPGALoader behavior: use the application's backup/readback checks rather than assuming an exit code proves power-on operation.

Validation: Windows x64 cross-compilation and synthetic transport tests. Tests cover ambiguous selection before open, serial selection, changed identity after open, partial writes and handle cleanup. `--scan-usb` checks installed DLL enumeration without opening a device. On the development Windows machine, the installed FTDI driver enumerated two Digilent FT232H cables; no-serial detection stopped with both identifiers, and serial-selected HS3 JTAG detection returned Xilinx Kintex-7 XC7K160T (IDCODE 0x0364c093), exit 0. No driver was changed. BIT loading, MCS programming/readback and subsequent Vivado reuse remain unverified. No connected hardware is programmed as part of the build.

Reference: https://ftdichip.com/document/programming-guides/

