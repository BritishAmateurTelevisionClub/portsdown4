#include "t2interp.h"
#include <cstdlib>
/*
 * dvb_t2_stack - narrowband DVB-T2 transmitter for the BATC Portsdown.
 * Drop-in companion to dvb_t_stack (same command line): takes the encoder's
 * transport stream on UDP, modulates DVB-T2 and drives a LimeSDR or Pluto.
 *
 * G8YTZ narrowband DVB-T2 project, GPLv3.
 *   DVB-T2 processing: gr-dtv, GNU Radio 3.10.9.2 (GPLv3, Ron Economos et al.)
 *   Lime / Pluto drivers: Portsdown 4 dvb_t_stack (GPLv3, Charles Brain G4GUO)
 *
 *   dvb_t2_stack -m qpsk -f 437000000 -b 2000000 -e 1/2 -g 1/8 -r lime -a 0.8 -p 1314
 *   dvb_t2_stack -c -m qpsk -b 2000000 -e 1/2 -g 1/8     (print TS capacity in b/s)
 */
#include "t2core.h"
#include "tsinput.h"
#include "radio/dvb_t.h"
#ifdef __USE_LIME__
#include "radio/lime.h"
#endif
#ifdef __USE_PLUTO__
#include "radio/pluto.h"
#endif
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>
#include <chrono>

static std::atomic<bool> g_run{ true };
static void on_signal(int) { g_run = false; }

static void help(const char* n)
{
    printf("\n%s - narrowband DVB-T2 transmitter (dvb_t_stack compatible)\n", n);
    printf("-m constellation qpsk 16qam 64qam (default qpsk)\n");
    printf("-f frequency in Hz (default 437000000)\n");
    printf("-b channel bandwidth in Hz: 1350000 1700000 2000000 ... (default 2000000)\n");
    printf("   1700000/1712000 use the DVB-T2 1.7 MHz clock (131/71 MHz), others 8/7 x bandwidth\n");
    printf("-e FEC 1/2 3/5 2/3 3/4 4/5 5/6 (7/8 is mapped to 5/6) (default 1/2)\n");
    printf("-g guard interval 1/4 1/8 1/16 1/32 (default 1/8)\n");
    printf("-r radio lime pluto (default lime); file:NAME writes IQ (cf32) for testing\n");
    printf("-a level: Lime normalised gain 0..1, Pluto attenuation dB <= 0 (default 0.8 / -10)\n");
    printf("-n Pluto network address (default 192.168.2.1)\n");
    printf("-p UDP port for the transport stream (default 1314)\n");
    printf("-i input control file (accepted for compatibility, ignored)\n");
    printf("-t transmission mode (accepted for compatibility; 2K is always used)\n");
    printf("-c print the TS capacity in bit/s for these settings and exit\n");
    printf("-h this help\n\n");
}

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IOLBF, 0);   // line-buffered for Portsdown logs
    T2Config cfg;
    cfg.bw = 2.0;
    std::string radio = "lime", naddr = "192.168.2.1";
    unsigned long long freq = 437000000ULL;
    double level = -999;
    int port = 1314;
    bool capacity_only = false;
    std::string dumpts;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto nx = [&]() { return std::string(i + 1 < argc ? argv[++i] : ""); };
        if (a == "-m") { cfg.mod = nx(); if (cfg.mod == "32qam") cfg.mod = "64qam"; }
        else if (a == "-f") freq = strtoull(nx().c_str(), nullptr, 10);
        else if (a == "-b") {
            double hz = atof(nx().c_str());
            cfg.bw = (hz == 1700000 || hz == 1712000) ? 1.7 : hz / 1e6;
        }
        else if (a == "-e") { cfg.rate = nx(); if (cfg.rate == "7/8") { cfg.rate = "5/6"; fprintf(stderr, "FEC 7/8 is not in DVB-T2: using 5/6\n"); } }
        else if (a == "-g") cfg.gi = nx();
        else if (a == "-r") radio = nx();
        else if (a == "-a") level = atof(nx().c_str());
        else if (a == "-n") naddr = nx();
        else if (a == "-p") port = atoi(nx().c_str());
        else if (a == "-i" || a == "-t") nx();
        else if (a == "-c") capacity_only = true;
        else if (a == "--frame-ms") cfg.frame_ms = atof(nx().c_str());
        else if (a == "--level") cfg.level = (float)atof(nx().c_str());
        else if (a == "--dump-ts") dumpts = nx();
        else if (a == "-h") { help(argv[0]); return 0; }
        else { fprintf(stderr, "unknown option %s (-h for help)\n", a.c_str()); return 1; }
    }

    T2Core core;
    std::string err;
    if (!core.configure(cfg, err)) { fprintf(stderr, "dvb_t2_stack: %s\n", err.c_str()); return 1; }
    if (capacity_only) { printf("%.0f\n", core.ch_rate); return 0; }
    printf("%s\n", core.describe().c_str());
    printf("Sample Rate %.0f\n", core.fs);
    printf("Channel capacity %.0f bps\n", core.ch_rate);
    printf("Tx Frequency %llu\n", freq);

    DVBTFormat fmt{};
    fmt.tx_sample_rate = (uint32_t)(core.fs + 0.5);
    int interp_L = 1;
    fmt.freq = freq;
    fmt.port = (uint16_t)port;
    snprintf(fmt.n_addr, sizeof fmt.n_addr, "%s", naddr.c_str());

    std::function<void(scmplx*, int)> tx;
    std::function<void()> close_radio = [] {};
    FILE* iqf = nullptr;
    bool paced_null = false;
    if (radio == "null") paced_null = true;      // test: no radio, paced like one (1 s FIFO)
    else if (radio.rfind("file:", 0) == 0) {
        iqf = fopen(radio.c_str() + 5, "wb");
        if (!iqf) { perror(radio.c_str() + 5); return 1; }
    }
#ifdef __USE_LIME__
    else if (radio == "lime") {
        fmt.radio = R_LIME;
        // Software interpolation: moves the sample-rate images well away from the signal.
        // Aim for an output rate of at least 4.5 MS/s.
        interp_L = (int)std::ceil(4.5e6 / core.fs);
        if (interp_L < 1) interp_L = 1;
        if (const char* e = getenv("T2_INTERP")) { int v = atoi(e); if (v >= 1 && v <= 16) interp_L = v; }
        if (interp_L > 1) {
            fmt.tx_sample_rate = (uint32_t)(core.fs * interp_L + 0.5);
            fmt.chan_bw_hz = 0;                  // no Lime GFIR when we filter in software
            fprintf(stderr, "T2 software interpolation x%d: Lime sample rate %u\n", interp_L, fmt.tx_sample_rate);
        }
        fmt.level = (float)(level == -999 ? 0.8 : level);
        if (lime_open(&fmt) != 0) { fprintf(stderr, "LimeSDR not opened\n"); return 1; }
        tx = [](scmplx* s, int n) { lime_transmit_samples(s, n); };
        close_radio = [] { lime_close(); };
    }
#endif
#ifdef __USE_PLUTO__
    else if (radio == "pluto") {
        fmt.radio = R_PLUTO;
        fmt.level = (float)(level == -999 ? -10 : level);
        if (pluto_open(&fmt, 32768 + 8192) < 0) return 1;
        if (fmt.tx_sample_rate > 3000000) pluto_set_tx_sample_rate(fmt.tx_sample_rate);
        else { pluto_set_tx_sample_rate(fmt.tx_sample_rate * 8); }
//        else { pluto_set_tx_sample_rate(fmt.tx_sample_rate * 8); pluto_configure_x8_int_dec(fmt.tx_sample_rate); }
	pluto_configure_x8_int_dec(fmt.tx_sample_rate);
        pluto_set_tx_frequency((long long)fmt.freq);
        if (fmt.level <= 0 && fmt.level > -89.75) pluto_set_tx_level(fmt.level);
        pluto_start_tx_stream();
        tx = [](scmplx* s, int n) { pluto_tx_samples(s, n); };
        close_radio = [] { pluto_stop_tx_stream(); pluto_close(); };
    }
#endif
    else { fprintf(stderr, "unknown or unsupported radio '%s'\n", radio.c_str()); return 1; }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    T2Interp t2ip(interp_L);
    std::vector<std::complex<float>> ipin, ipout;
    LiveTS lts;
    lts.set_channel_rate(core.ch_rate);
    int sock = lts.open_udp(port);
    if (sock < 0) { close_radio(); return 1; }
    std::thread([&lts, sock] { lts.from_udp_socket(sock); }).detach();
    printf("Listening for TS on UDP port %d\n", port);

    std::vector<scmplx> sbuf(8192);
    unsigned long last = 0;
    size_t dump_limit = (size_t)-1;
    if (dumpts.empty() && access("/tmp/t2dump", F_OK) == 0) {       // touch /tmp/t2dump to enable
        dumpts = "/tmp/t2dump.ts";
        dump_limit = (size_t)(core.ch_rate / 8 * 60);                // stops by itself after 60 s
    }
    FILE* dump = dumpts.empty() ? nullptr : fopen(dumpts.c_str(), "wb");
    size_t dumped = 0;
    if (dump) printf("Recording the transmitted TS to %s%s\n", dumpts.c_str(),
                     dump_limit != (size_t)-1 ? " (first 60 s)" : "");
    size_t nout = 0;
    auto t0 = std::chrono::steady_clock::now();
    core.run(
        [&](std::vector<char>& dst) {
            size_t before = dst.size();
            lts.take(dst);
            if (dump) {
                size_t len = dst.size() - before;
                fwrite(dst.data() + before, 1, len, dump);
                dumped += len;
                if (dumped >= dump_limit) { fclose(dump); dump = nullptr; printf("TS recording finished\n"); }
            }
            unsigned long tot = lts.real + lts.nulls;
            if (tot - last > 20000) {
                last = tot;
                fprintf(stderr, "TS in %lu pkts (%lu encoder nulls removed), %.1f%% nulls added, %lu dropped, %zu queued, buffered %.2f s, %lu slips, %lu resyncs\n",
                        (unsigned long)lts.real, (unsigned long)lts.in_nulls, 100.0 * lts.nulls / tot,
                        (unsigned long)lts.dropped, lts.queued(), lts.buffered_s(), (unsigned long)lts.slips, (unsigned long)lts.resyncs);
            }
        },
        [&](const gr_complex* p, size_t n) {
            if (iqf) return fwrite(p, sizeof(gr_complex), n, iqf) == n && g_run;
            if (paced_null) {
                nout += n;
                auto due = t0 + std::chrono::duration<double>(nout / core.fs - 1.0);
                std::this_thread::sleep_until(due);
                return (bool)g_run;
            }
            if (interp_L > 1) {
                // Interpolate in chunks of at most sbuf.size()/interp_L input samples
                const size_t chunk = sbuf.size() / interp_L;
                for (size_t k = 0; k < n && g_run; k += chunk) {
                    size_t m = std::min(chunk, n - k);
                    ipin.resize(m);
                    for (size_t j = 0; j < m; j++) ipin[j] = std::complex<float>(p[k + j].real(), p[k + j].imag());
                    t2ip.process(ipin.data(), m, ipout);
                    for (size_t j = 0; j < ipout.size(); j++) {
                        float re = ipout[j].real() * 32767.0f, im = ipout[j].imag() * 32767.0f;
                        sbuf[j].re = (short)std::max(-32767.0f, std::min(32767.0f, re));
                        sbuf[j].im = (short)std::max(-32767.0f, std::min(32767.0f, im));
                    }
                    tx(sbuf.data(), (int)ipout.size());
                }
                return (bool)g_run;
            }
            for (size_t k = 0; k < n && g_run; k += sbuf.size()) {
                size_t m = std::min(sbuf.size(), n - k);
                for (size_t j = 0; j < m; j++) {
                    float re = p[k + j].real() * 32767.0f, im = p[k + j].imag() * 32767.0f;
                    sbuf[j].re = (short)std::max(-32767.0f, std::min(32767.0f, re));
                    sbuf[j].im = (short)std::max(-32767.0f, std::min(32767.0f, im));
                }
                tx(sbuf.data(), (int)m);
            }
            return (bool)g_run;
        });
    close_radio();
    if (iqf) fclose(iqf);
    if (dump) fclose(dump);
    fprintf(stderr, "TS in %lu pkts (%lu encoder nulls removed), %lu nulls added, %lu dropped\n",
            (unsigned long)lts.real, (unsigned long)lts.in_nulls, (unsigned long)lts.nulls, (unsigned long)lts.dropped);
    printf("DVB-T2 exited\n");
    return 0;
}
