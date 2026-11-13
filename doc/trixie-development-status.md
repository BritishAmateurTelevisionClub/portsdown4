# Experimental Pi 4/Trixie and DATV-Express DVB-S2 work

Status as of 12 November 2026. This note shares development results for
maintainer review; it does **not** change Portsdown's installer, GUI, or RF
behavior. The experimental source is on the
[`mathisono/portsdown4_NTSC` `arm64-transmitter` branch](https://github.com/mathisono/portsdown4_NTSC/tree/arm64-transmitter).

## Two Trixie targets

| Target | Demonstrated | Still needed |
| --- | --- | --- |
| Raspberry Pi 4, **32-bit Trixie (`armhf`)**, original 7-inch 800x480 display | FFmpeg NTSC/PAL MPEG-TS path for analog/webcam input; configurable software/hardware encoder; initial framebuffer/touchscreen fixes; offline TS tests. The legacy ARM32 `rpidatv` core cross-compiled to an `armhf` ELF on a 64-bit Pi, but was not run. Hardware H.264 encoding was demonstrated on a separate **64-bit** Pi 4, not on the 32-bit target. | Test on the target image with display, capture and RF hardware; finish the modern installer, GUI, remaining video modes and update path. |
| Raspberry Pi 4, **64-bit Trixie (`arm64`)** | Native AArch64 builds of the Portsdown `rpidatv` core, the DVB-T2 stack and upstream DVB-S `express_server`; offline DVB-S2 I/Q generation and a standalone DATV-Express raw-USB backend build; synthetic SRT-to-TS ingest. | Validate the Pi GPIO/DMA transmitter and output hardware, USB/firmware, sustained streaming, PTT, decoded RF, actual ATEM feed and GUI integration. No DATV-Express board was attached and no RF was transmitted. |

## Native Portsdown transmitter (Pi GPIO/IQ/RF path)

The existing `rpidatv` path used for Portsdown IQ/QPSKRF output is a **DVB-S**
transmitter, separate from the DATV-Express USB path. On 32-bit Trixie, the
legacy ARM32 core cross-compiled and the new TS/GUI work passed offline checks,
but the intended `armhf` Pi, touchscreen and RF chain have not been run. On
64-bit Trixie, a portable DVB-S encoder and Pi 4 pointer/revision fixes allowed
`rpidatv` to build and run its help path natively on an AArch64 Pi; its encoder
passed offline comparison tests. The GPIO/DMA/IQ/RF path was **not** exercised.

The native `rpidatv` work does **not** add DVB-S2. The existing Muntjac DVB-S2
path still uses ARM32 NEON assembly and pointer casts that need an arm64 port;
it has not been tested on Trixie. The standalone DVB-S2 I/Q result below is
for a different, experimental DATV-Express USB backend, not a native Pi RF
transmission or an integrated Portsdown DVB-S2 mode.

The existing Portsdown `DATVEXPRESS` GUI choice forces **DVB-S**, so an AArch64
build of `express_server` alone is not a DVB-S2 solution. The experimental
[`src/express_s2` backend](https://github.com/mathisono/portsdown4_NTSC/tree/arm64-transmitter/src/express_s2)
links F5OEO `libdvbmod` to the existing Linux raw-16 DATV-Express USB path.
It accepts MPEG-TS and produced DVB-S2 I/Q offline on AArch64, but has **not**
streamed to a board. Its hardware transmit option is explicit and unverified;
it is not connected to Portsdown's `videots` or GUI. The existing DVB-S mode
should remain separate.

An initial raw-USB asynchronous buffer ownership/error fix and a reproducible
[modulator probe](https://github.com/mathisono/portsdown4_NTSC/blob/arm64-transmitter/tests/express_s2_modulator_probe.cpp)
are also in the branch. A standalone
[SRT listener](https://github.com/mathisono/portsdown4_NTSC/blob/arm64-transmitter/scripts/srt_ts_listener.sh)
accepted a synthetic H.264/AAC stream; the real encoder feed and its bitrate
are not yet known. Neither 32-bit nor 64-bit Portsdown currently has verified
DVB-S2 transmission via DATV-Express.

**Hardware needed to continue testing:** I need access to a DATV-Express USB
board on TANK and a dummy load or suitably attenuated receiver. Without that
hardware, I cannot validate firmware loading, USB streaming, PTT, or decoded
DVB-S2 RF output. The offline results do not establish transmitter operation.

## Remaining work before an integration PR

1. Validate the 32-bit Trixie display, touch, capture, H.264 encoder and native
   Portsdown RF paths on the intended Pi 4, then finish and test a non-destructive installer.
2. Validate the 64-bit Pi 4 native `rpidatv` DMA/IQ/RF path and port remaining
   ARM32-only modules, including the Muntjac DVB-S2 encoder.
3. With the DATV-Express board connected to a dummy load or attenuated
   receiver, check raw-16 firmware, USB pacing/error recovery, PTT/stop and a
   decodable DVB-S2 signal across representative symbol rates and FEC modes.
4. Integrate MPEG-TS input and a separate DVB-S2 GUI choice only after the
   hardware path works; measure the actual SRT source and match channel rate.

The fork's [64-bit transmitter details](https://github.com/mathisono/portsdown4_NTSC/blob/arm64-transmitter/doc/arm64-transmitter.md),
[32-bit migration notes](https://github.com/mathisono/portsdown4_NTSC/blob/arm64-transmitter/doc/ntsc-trixie-migration.md),
and [SRT results](https://github.com/mathisono/portsdown4_NTSC/blob/arm64-transmitter/doc/srt-express-ingest.md)
contain build commands, observations and fuller acceptance gates. Feedback on
the proposed DVB-S2/DATV-Express integration boundary is welcome.
