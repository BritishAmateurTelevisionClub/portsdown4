# Narrowband DVB-T2 for the Portsdown - dvb_t2_stack and t2mod

G8YTZ narrowband DVB-T2 project. GPLv3.

* **DVB-T2 signal processing:** the unmodified gr-dtv code from GNU Radio
  3.10.9.2 (GPLv3, Ron Economos and contributors), built against a tiny runtime
  shim (`shim/`) - no GNU Radio needed.
* **Lime and Pluto drivers:** `radio/lime.cpp` and `radio/pluto.cpp` from the
  Portsdown 4 `dvb_t_stack` (GPLv3, Charles Brain G4GUO), unchanged apart from
  the LimeSuite include path, via a small `radio/dvb_t.h` compatibility header.

## Programs
* **dvb_t2_stack** - Portsdown drop-in: same command line as `dvb_t_stack`,
  transport stream on UDP (port 1314), DVB-T2 out to a LimeSDR or Pluto.
  `-c` prints the TS capacity (b/s) so a launch script can set the encoder rate.
* **t2mod** - general tool: TS file / stdin / UDP in, complex-float IQ out
  (file or stdout). Used with `iqplay.py` (GNU Radio + SoapySDR) for testing.

## Mode
2K FFT, SISO, rotated constellations, normal FEC frames, T2 frames sized
automatically to <= 250 ms. Guard 1/4 (PP1), 1/8 (PP2), 1/16 and 1/32 (PP4).
QPSK/16QAM/64QAM, LDPC 1/2 3/5 2/3 3/4 4/5 5/6. Bandwidth 1.7 MHz uses the
T2 131/71 MHz clock; any other value uses 8/7 x bandwidth (e.g. 1.35, 2 MHz).
Live input: the encoder's own null packets are removed, the encoder's rate is
measured from its PCRs, and each real packet is sent at its intended time with
nulls filling the gaps. This keeps PCR timing within about 1 ms (T2 frames are
built in 250 ms bursts, so simple "nulls when the queue is empty" gives ~125 ms
PCR jitter and receivers show the service but rarely a picture). Any encoder
mux rate works as long as its real content fits the T2 capacity.
To record exactly what is being transmitted when the Portsdown launches it:
`touch /tmp/t2dump` before transmitting; the first 60 s of TS is written to
/tmp/t2dump.ts (play it in VLC). Remove /tmp/t2dump afterwards.
Test tools: `-r null` (paced dummy radio) and `--dump-ts file.ts`, then
`python3 jitter.py file.ts $(./dvb_t2_stack -c ...)` to measure PCR timing.

## Verified
IQ output matches the GNU Radio flowgraph to within 9e-8 on every sample for
1.35/1.7/2 MHz, QPSK/16QAM/64QAM, rates 1/2-5/6, all four guard intervals.
On air (Lime Mini, Pi 4): locked by a BATC Knucker/Ryde (2 MHz) with full video.

## Build
    # Portsdown 4 (Buster, Pi 4) - dependencies are already installed:
    make PI4=1
    # Raspberry Pi OS Trixie / Debian:
    sudo apt install g++ make libfftw3-dev libiio-dev liblimesuite-dev
    make

## dvb_t2_stack options (dvb_t_stack compatible)
    -m qpsk|16qam|64qam   -f Hz   -b Hz   -e 1/2..5/6 (7/8 -> 5/6)   -g 1/4|1/8|1/16|1/32
    -r lime|pluto|file:NAME   -a level (Lime 0..1, Pluto dB <= 0)   -n Pluto IP
    -p UDP port   -i/-t accepted and ignored   -c print capacity   -h help

## Portsdown integration
Selected by the DVB-T / DVB-T2 buttons in the DVB-T Parameters menu (16);
`a_dvb-t.sh` chooses dvb_t2_stack and sets the encoder rate from `-c`.
See DVBT2_INTEGRATION.md in the patch package.
