# Fork maintenance instructions

Keep the upstream libftdi path unchanged unless USE_FTDI_D2XX is enabled. The optional Windows x64 D2XX path is scoped to FT232H interface A / HS3. Never silently treat Xilinx Platform Cable as FTDI, swap drivers, install a driver, or load vendor DLLs from the current working directory. Scan is enumeration only. Fail before opening on ambiguous serials and recheck identity immediately after open, before configuration. Preserve bounded reads, short-write failures, handle cleanup, and no automatic programming retries. Keep synthetic transport tests distinct from physical JTAG/flash/Vivado compatibility evidence. Never program connected hardware as a build test.

See WINDOWS-D2XX.md / WINDOWS-D2XX.ja.md. Keep commands synchronized and source/comments/errors English. Proprietary drivers/DLLs, local device backups and firmware dumps must not be committed. Build outputs are not source files. This fork does not yet integrate its experimental backend into the AMANEQ GUI distribution.

