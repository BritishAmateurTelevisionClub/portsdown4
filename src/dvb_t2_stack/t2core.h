/*
 * t2core - narrowband DVB-T2 modulator core (G8YTZ, GPLv3).
 * Wraps the unmodified gr-dtv DVB-T2 blocks (GNU Radio 3.10.9.2, GPLv3,
 * Ron Economos et al.) in a simple pull pipeline with no GNU Radio runtime.
 */
#pragma once
#include <gnuradio/types.h>
#include <gnuradio/block.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct T2Config {
    double bw = 1.7;            // MHz: 1.7 uses the T2 131/71 MHz clock, else 8/7 x bw
    std::string mod = "qpsk";   // qpsk | 16qam | 64qam
    std::string rate = "1/2";   // 1/2 3/5 2/3 3/4 4/5 5/6
    std::string gi = "1/8";     // 1/4 (PP1) 1/8 (PP2) 1/16 (PP4) 1/32 (PP4)
    double frame_ms = 250;      // T2 frame length limit
    float level = 0.2f;         // output RMS (full scale = 1.0)
};

class T2Core {
public:
    bool configure(const T2Config& c, std::string& err);
    /* Run until out() returns false.
     * feed(dst): append at least one 188-byte TS packet to dst (called when starved).
     * out(p, n): n complex samples at fs, already scaled by level. */
    void run(const std::function<void(std::vector<char>&)>& feed,
             const std::function<bool(const gr_complex*, size_t)>& out);
    double fs = 0, frame_s = 0, ch_rate = 0;   // sample rate, frame length, TS payload rate
    int ndata = 0, fecblocks = 0, fft = 2048, gi_len = 256;
    std::string describe() const;
private:
    struct Stage;
    std::vector<std::shared_ptr<gr::block>> m_blocks;
    T2Config m_cfg;
};

double t2_fs_for_bw(double bw_mhz);
