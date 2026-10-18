/* t2core - see t2core.h (G8YTZ, GPLv3) */
#include "t2core.h"
#include <gnuradio/dtv/dvb_bbheader_bb.h>
#include <gnuradio/dtv/dvb_bbscrambler_bb.h>
#include <gnuradio/dtv/dvb_bch_bb.h>
#include <gnuradio/dtv/dvb_ldpc_bb.h>
#include <gnuradio/dtv/dvbt2_interleaver_bb.h>
#include <gnuradio/dtv/dvbt2_modulator_bc.h>
#include <gnuradio/dtv/dvbt2_cellinterleaver_cc.h>
#include <gnuradio/dtv/dvbt2_framemapper_cc.h>
#include <gnuradio/dtv/dvbt2_freqinterleaver_cc.h>
#include <gnuradio/dtv/dvbt2_pilotgenerator_cc.h>
#include <gnuradio/dtv/dvbt2_p1insertion_cc.h>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace gr::dtv;

double t2_fs_for_bw(double bw) { return std::fabs(bw - 1.7) < 1e-6 ? 131e6 / 71.0 : bw * 8e6 / 7.0; }

/* One pipeline stage: a gr-dtv block plus its pending input. */
struct T2Core::Stage {
    gr::block* b;
    std::vector<char> in;
    size_t rd = 0;
    std::vector<char> out;
    size_t avail() const { return (in.size() - rd) / b->in_size(); }
    void push(const char* p, size_t n) {
        if (rd > (1u << 22) && rd > in.size() / 2) { in.erase(in.begin(), in.begin() + rd); rd = 0; }
        in.insert(in.end(), p, p + n);
    }
    size_t step(std::vector<char>& dst) {
        int nom = b->output_multiple();
        gr_vector_int req(1);
        b->forecast(nom, req);
        size_t have = avail();
        if (have < (size_t)req[0]) return 0;
        out.resize((size_t)nom * b->out_size());
        gr_vector_int nin{ (int)have };
        gr_vector_const_void_star ip{ in.data() + rd };
        gr_vector_void_star op{ out.data() };
        b->d_consumed = 0;
        int n = b->general_work(nom, nin, ip, op);
        rd += (size_t)b->d_consumed * b->in_size();
        size_t bytes = (size_t)std::max(n, 0) * b->out_size();
        dst.insert(dst.end(), out.data(), out.data() + bytes);
        return bytes + (size_t)b->d_consumed;
    }
};

bool T2Core::configure(const T2Config& c, std::string& err)
{
    m_cfg = c;
    dvb_constellation_t cmod; int bpc;
    if (c.mod == "qpsk") { cmod = MOD_QPSK; bpc = 2; }
    else if (c.mod == "16qam") { cmod = MOD_16QAM; bpc = 4; }
    else if (c.mod == "64qam") { cmod = MOD_64QAM; bpc = 6; }
    else { err = "constellation must be qpsk, 16qam or 64qam"; return false; }
    dvb_code_rate_t code; int kbch;
    if (c.rate == "1/2") { code = C1_2; kbch = 32208; }
    else if (c.rate == "3/5") { code = C3_5; kbch = 38688; }
    else if (c.rate == "2/3") { code = C2_3; kbch = 43040; }
    else if (c.rate == "3/4") { code = C3_4; kbch = 48408; }
    else if (c.rate == "4/5") { code = C4_5; kbch = 51648; }
    else if (c.rate == "5/6") { code = C5_6; kbch = 53840; }
    else { err = "FEC must be 1/2, 3/5, 2/3, 3/4, 4/5 or 5/6"; return false; }
    /* 2K SISO: allowed pilot patterns per guard interval (EN 302 755) */
    dvb_guardinterval_t gi; dvbt2_pilotpattern_t pp; int c_data, n_fc, c_fc;
    if (c.gi == "1/4")       { gi = GI_1_4;  gi_len = fft / 4;  pp = PILOT_PP1; c_data = 1522; n_fc = 1136; c_fc = 804; }
    else if (c.gi == "1/8")  { gi = GI_1_8;  gi_len = fft / 8;  pp = PILOT_PP2; c_data = 1532; n_fc = 1420; c_fc = 1309; }
    else if (c.gi == "1/16") { gi = GI_1_16; gi_len = fft / 16; pp = PILOT_PP4; c_data = 1602; n_fc = 1562; c_fc = 1415; }
    else if (c.gi == "1/32") { gi = GI_1_32; gi_len = fft / 32; pp = PILOT_PP4; c_data = 1602; n_fc = 1562; c_fc = 1415; }
    else { err = "guard interval must be 1/4, 1/8, 1/16 or 1/32"; return false; }
    const int N_P2 = 8, C_P2 = 1118;

    fs = t2_fs_for_bw(c.bw);
    int sym = fft + gi_len;
    ndata = (int)std::floor((c.frame_ms / 1000.0 * fs - fft) / sym) - N_P2;
    ndata = std::max(8, std::min(ndata, 2000));
    long cells = (long)N_P2 * C_P2 + (long)(ndata - 1) * c_data + n_fc - 1840 - (n_fc - c_fc) - 4000;
    fecblocks = (int)(cells / (64800 / bpc));
    if (fecblocks < 1) { err = "T2 frame too short for one FEC block"; return false; }
    frame_s = (fft + (double)(N_P2 + ndata) * sym) / fs;
    ch_rate = fecblocks * (kbch - 80) / frame_s;

    m_blocks = {
        dvb_bbheader_bb::make(STANDARD_DVBT2, FECFRAME_NORMAL, code, RO_0_35, INPUTMODE_NORMAL,
                              INBAND_OFF, fecblocks, 4000000),
        dvb_bbscrambler_bb::make(STANDARD_DVBT2, FECFRAME_NORMAL, code),
        dvb_bch_bb::make(STANDARD_DVBT2, FECFRAME_NORMAL, code),
        dvb_ldpc_bb::make(STANDARD_DVBT2, FECFRAME_NORMAL, code, MOD_OTHER),
        dvbt2_interleaver_bb::make(FECFRAME_NORMAL, code, cmod),
        dvbt2_modulator_bc::make(FECFRAME_NORMAL, cmod, ROTATION_ON),
        dvbt2_cellinterleaver_cc::make(FECFRAME_NORMAL, cmod, fecblocks, 1),
        dvbt2_framemapper_cc::make(FECFRAME_NORMAL, code, cmod, ROTATION_ON, fecblocks, 1,
                                   CARRIERS_NORMAL, FFTSIZE_2K, gi, L1_MOD_BPSK, pp, 2, ndata,
                                   PAPR_OFF, VERSION_111, PREAMBLE_T2_SISO, INPUTMODE_NORMAL,
                                   RESERVED_OFF, L1_SCRAMBLED_OFF, INBAND_OFF),
        dvbt2_freqinterleaver_cc::make(CARRIERS_NORMAL, FFTSIZE_2K, pp, gi, ndata, PAPR_OFF,
                                       VERSION_111, PREAMBLE_T2_SISO),
        dvbt2_pilotgenerator_cc::make(CARRIERS_NORMAL, FFTSIZE_2K, pp, gi, ndata, PAPR_OFF,
                                      VERSION_111, PREAMBLE_T2_SISO, MISO_TX1, EQUALIZATION_OFF,
                                      BANDWIDTH_1_7_MHZ, fft),
        dvbt2_p1insertion_cc::make(CARRIERS_EXTENDED, FFTSIZE_2K, gi, ndata, PREAMBLE_T2_SISO,
                                   SHOWLEVELS_OFF, 3.3f)
    };
    return true;
}

std::string T2Core::describe() const
{
    char b[256];
    snprintf(b, sizeof b, "DVB-T2 %.3g MHz Fs=%.0f %s %s GI %s | frame %.1f ms, %d data symbols, "
             "%d FEC blocks | TS capacity %.3f Mb/s", m_cfg.bw, fs, m_cfg.mod.c_str(),
             m_cfg.rate.c_str(), m_cfg.gi.c_str(), frame_s * 1e3, ndata, fecblocks, ch_rate / 1e6);
    return b;
}

void T2Core::run(const std::function<void(std::vector<char>&)>& feed,
                 const std::function<bool(const gr_complex*, size_t)>& out)
{
    std::vector<Stage> front;
    for (size_t i = 0; i + 1 < m_blocks.size(); i++) front.push_back(Stage{ m_blocks[i].get() });
    Stage p1s{ m_blocks.back().get() };
    std::vector<char> tmp, sym, outs;
    const size_t symlen = fft + gi_len;
    for (;;) {
        bool progress = false;
        for (size_t i = 0; i < front.size(); i++) {
            tmp.clear();
            while (front[i].step(tmp)) progress = true;
            if (tmp.empty()) continue;
            if (i + 1 < front.size()) { front[i + 1].push(tmp.data(), tmp.size()); continue; }
            /* cyclic prefix: symbol -> last gi_len samples + symbol */
            const gr_complex* v = reinterpret_cast<const gr_complex*>(tmp.data());
            size_t nsym = tmp.size() / (sizeof(gr_complex) * fft);
            sym.resize(nsym * symlen * sizeof(gr_complex));
            gr_complex* o = reinterpret_cast<gr_complex*>(sym.data());
            for (size_t s = 0; s < nsym; s++) {
                memcpy(o + s * symlen, v + s * fft + (fft - gi_len), gi_len * sizeof(gr_complex));
                memcpy(o + s * symlen + gi_len, v + s * fft, fft * sizeof(gr_complex));
            }
            p1s.push(sym.data(), sym.size());
        }
        outs.clear();
        while (p1s.step(outs)) progress = true;
        if (!outs.empty()) {
            gr_complex* o = reinterpret_cast<gr_complex*>(outs.data());
            size_t n = outs.size() / sizeof(gr_complex);
            for (size_t k = 0; k < n; k++) o[k] *= m_cfg.level;
            if (!out(o, n)) return;
        }
        if (!progress) {
            tmp.clear();
            feed(tmp);
            front[0].push(tmp.data(), tmp.size());
        }
    }
}
