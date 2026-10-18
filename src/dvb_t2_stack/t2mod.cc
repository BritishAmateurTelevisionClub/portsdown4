/*
 * t2mod - narrowband DVB-T2 modulator: TS (file, stdin or UDP) -> complex IQ.
 * G8YTZ narrowband DVB-T2 project, GPLv3. Signal processing: gr-dtv (GPLv3).
 */
#include "t2core.h"
#include "tsinput.h"
#include <cstdlib>
#include <thread>

int main(int argc, char** argv)
{
    T2Config c;
    std::string ts, outpath;
    double seconds = 5;
    bool nopad = false;
    int udp = 0;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto nx = [&]() { return std::string(i + 1 < argc ? argv[++i] : ""); };
        if (a == "--ts") ts = nx();
        else if (a == "--udp") udp = atoi(nx().c_str());
        else if (a == "--out") outpath = nx();
        else if (a == "--bw") c.bw = atof(nx().c_str());
        else if (a == "--mod") c.mod = nx();
        else if (a == "--rate") c.rate = nx();
        else if (a == "--gi") c.gi = nx();
        else if (a == "--frame-ms") c.frame_ms = atof(nx().c_str());
        else if (a == "--seconds") seconds = atof(nx().c_str());
        else if (a == "--level") c.level = (float)atof(nx().c_str());
        else if (a == "--no-pad") nopad = true;
        else { fprintf(stderr, "unknown option %s\n", a.c_str()); return 1; }
    }
    if ((ts.empty() && !udp) || outpath.empty()) {
        fprintf(stderr,
            "usage: t2mod (--ts file.ts | --ts - | --udp PORT) --out iq.cf32|- [options]\n"
            "  --bw 1.7|1.35|2 (MHz)  --mod qpsk|16qam|64qam  --rate 1/2|3/5|2/3|3/4|4/5|5/6\n"
            "  --gi 1/4|1/8|1/16|1/32  --frame-ms 250  --seconds 5 (0 = forever)  --level 0.2  --no-pad\n");
        return 1;
    }
    T2Core core;
    std::string err;
    if (!core.configure(c, err)) { fprintf(stderr, "t2mod: %s\n", err.c_str()); return 1; }
    fprintf(stderr, "t2mod: %s\n", core.describe().c_str());

    const bool live = udp || ts == "-";
    LiveTS lts;
    lts.set_channel_rate(core.ch_rate);
    std::vector<char> file;
    size_t fpos = 0;
    if (live) {
        if (udp) {
            int s = lts.open_udp(udp);
            if (s < 0) return 1;
            std::thread([&lts, s] { lts.from_udp_socket(s); }).detach();
        } else std::thread([&lts] { lts.from_fd(0); }).detach();
        fprintf(stderr, "t2mod: live TS from %s\n", udp ? ("UDP port " + std::to_string(udp)).c_str() : "stdin");
    } else {
        FILE* f = fopen(ts.c_str(), "rb");
        if (!f) { perror(ts.c_str()); return 1; }
        char buf[65536]; size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) file.insert(file.end(), buf, buf + n);
        fclose(f);
        file.resize(file.size() / 188 * 188);
        if (file.empty()) { fprintf(stderr, "t2mod: empty TS\n"); return 1; }
        double fr = nopad ? 0 : ts_pcr_rate(file);
        if (fr > 0 && fr / core.ch_rate < 0.9999) {
            file = ts_pad(file, fr / core.ch_rate);
            fprintf(stderr, "t2mod: TS %.3f Mb/s padded with nulls to channel rate\n", fr / 1e6);
        } else if (fr > core.ch_rate * 1.0001)
            fprintf(stderr, "t2mod: WARNING TS %.3f Mb/s exceeds capacity %.3f Mb/s\n", fr / 1e6, core.ch_rate / 1e6);
    }

    FILE* of = (outpath == "-") ? stdout : fopen(outpath.c_str(), "wb");
    if (!of) { perror(outpath.c_str()); return 1; }
    static char obuf[1 << 20];
    setvbuf(of, obuf, _IOFBF, sizeof obuf);
    const size_t target = seconds > 0 ? (size_t)(seconds * core.fs) : (size_t)-1;
    size_t written = 0;
    unsigned long last = 0;

    core.run(
        [&](std::vector<char>& dst) {
            if (live) {
                lts.take(dst);
                unsigned long tot = lts.real + lts.nulls;
                if (tot - last > 5000) {
                    last = tot;
                    fprintf(stderr, "t2mod: live %lu pkts, %.1f%% nulls, %lu dropped   \r",
                            (unsigned long)lts.real, 100.0 * lts.nulls / tot, (unsigned long)lts.dropped);
                }
            } else {
                for (int k = 0; k < 256; k++) {
                    dst.insert(dst.end(), &file[fpos], &file[fpos] + 188);
                    fpos = (fpos + 188) % file.size();
                }
            }
        },
        [&](const gr_complex* p, size_t n) {
            size_t w = std::min(n, target - written);
            if (fwrite(p, sizeof(gr_complex), w, of) != w) return false;
            written += w;
            return written < target;
        });
    fflush(of);
    if (of != stdout) fclose(of);
    fprintf(stderr, "\nt2mod: wrote %zu samples (%.2f s)\n", written, written / core.fs);
    return 0;
}
