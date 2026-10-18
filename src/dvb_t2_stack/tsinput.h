/* Transport-stream sources for t2mod / dvb_t2_stack (G8YTZ, GPLv3). */
#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

static const unsigned char TS_NULL_HDR[4] = { 0x47, 0x1f, 0xff, 0x10 };

/* Mux rate of a CBR TS from its first and last PCR (b/s), or 0. */
inline double ts_pcr_rate(const std::vector<char>& ts)
{
    const unsigned char* d = reinterpret_cast<const unsigned char*>(ts.data());
    size_t n = ts.size() / 188;
    long first = -1, last = -1, pid0 = -1;
    for (size_t i = 0; i < n; i++) {
        const unsigned char* p = d + i * 188;
        if (p[0] != 0x47 || !(p[3] & 0x20) || p[4] == 0 || !(p[5] & 0x10)) continue;
        long pid = ((p[1] & 0x1f) << 8) | p[2];
        if (pid0 < 0) pid0 = pid;
        if (pid != pid0) continue;
        if (first < 0) first = (long)i;
        last = (long)i;
    }
    if (first < 0 || last <= first) return 0;
    auto pcr = [&](long i) {
        const unsigned char* b = d + i * 188 + 6;
        long long base = ((long long)b[0] << 25) | (b[1] << 17) | (b[2] << 9) | (b[3] << 1) | (b[4] >> 7);
        return base * 300 + (((b[4] & 1) << 8) | b[5]);
    };
    double dt = (double)(pcr(last) - pcr(first)) / 27e6;
    return dt > 0 ? (double)(last - first) * 188 * 8 / dt : 0;
}

/* Interleave null packets so the TS runs at exactly 1/ratio of its own rate. */
inline std::vector<char> ts_pad(const std::vector<char>& ts, double ratio)
{
    size_t n = ts.size() / 188, m = (size_t)std::llround(n / ratio);
    std::vector<char> out(m * 188);
    size_t used = 0;
    for (size_t k = 0; k < m; k++) {
        size_t take = std::min((size_t)std::floor((k + 1) * ratio), n);
        char* o = &out[k * 188];
        if (take > used) { memcpy(o, &ts[used * 188], 188); used++; }
        else { memset(o, 0, 188); memcpy(o, TS_NULL_HDR, 4); }
    }
    return out;
}

/* Live TS (stdin pipe or UDP) with timing-accurate null insertion.
 *
 * A reader thread queues the encoder's packets, dropping its own null padding
 * but remembering each packet's position in the original constant-rate stream.
 * The encoder's rate is measured from its PCRs, so every real packet has an
 * intended time. take() fills output slots at the T2 channel rate: a real
 * packet goes out when its time comes, nulls fill the gaps. This keeps the
 * PCR timing intact (T2 frames are pulled in 250 ms bursts; plain "nulls when
 * empty" gives ~100 ms PCR jitter and decoders stutter). If packets arrive
 * late the schedule slips later once and then holds. */
struct LiveTS {
    struct Item { std::array<char, 188> p; unsigned long long n; };
    std::mutex m;
    std::deque<Item> q;
    std::atomic<bool> eof{ false };
    std::atomic<unsigned long> real{ 0 }, nulls{ 0 }, dropped{ 0 }, in_nulls{ 0 }, slips{ 0 };
    bool strip_nulls = true;
    std::vector<char> acc;
    unsigned long long n_in = 0;          // packets seen from the encoder, incl. nulls
    int pcr_pid = -1;
    double n_a = 0, t_a = -1, t_last = -1; // PCR anchor (packet index, seconds) and last PCR
    std::atomic<double> rate_pps{ 0 };    // encoder packets per second, from PCRs since the anchor
    std::atomic<bool> reanchor{ false };  // encoder timing jumped: re-align the schedule
    double slot_s = 0;                    // one output slot in seconds
    unsigned long long k_out = 0;         // output slots issued
    double c = 0; bool c_set = false;     // schedule offset (seconds)
    std::atomic<unsigned long> resyncs{ 0 };
    double buffered_s() { double r = rate_pps; std::lock_guard<std::mutex> g(m); return (r > 0 && !q.empty()) ? (double)(q.back().n - q.front().n) / r : 0; }

    void set_channel_rate(double bps) { slot_s = 188.0 * 8.0 / bps; }

    /* Stream time of packet n (seconds, may be negative before the anchor). */
    double stream_time(unsigned long long n, double r) const { return t_a + ((double)n - n_a) / r; }

    void note_pcr(const char* p, unsigned long long n) {
        const unsigned char* u = reinterpret_cast<const unsigned char*>(p);
        if (!(u[3] & 0x20) || u[4] == 0 || !(u[5] & 0x10)) return;
        int pid = ((u[1] & 0x1f) << 8) | u[2];
        if (pcr_pid < 0) pcr_pid = pid;
        if (pid != pcr_pid) return;
        const unsigned char* b = u + 6;
        long long base = ((long long)b[0] << 25) | (b[1] << 17) | (b[2] << 9) | (b[3] << 1) | (b[4] >> 7);
        double t = (base * 300.0 + (((b[4] & 1) << 8) | b[5])) / 27e6;
        bool jump = t_last >= 0 && (t < t_last || t - t_last > 1.0);   // encoder restart / discontinuity
        t_last = t;
        if (t_a < 0 || jump) {
            std::lock_guard<std::mutex> g(m);
            n_a = (double)n; t_a = t;
            if (jump) { reanchor = true; resyncs++; }
            return;
        }
        if (t - t_a > 0.2) {
            double r = ((double)n - n_a) / (t - t_a);
            if (r > 50 && r < 200000) rate_pps = r;
        }
    }
    void add(const char* b, size_t len) {
        acc.insert(acc.end(), b, b + len);
        size_t i = 0;
        while (acc.size() - i >= 188) {
            if ((unsigned char)acc[i] != 0x47) { i++; continue; }
            unsigned long long n = n_in++;
            note_pcr(&acc[i], n);
            if (strip_nulls && (acc[i + 1] & 0x1f) == 0x1f && (unsigned char)acc[i + 2] == 0xff) {
                i += 188; in_nulls++; continue;
            }
            Item it; memcpy(it.p.data(), &acc[i], 188); it.n = n; i += 188;
            std::lock_guard<std::mutex> g(m);
            if (q.size() < 50000) q.push_back(it); else dropped++;
        }
        acc.erase(acc.begin(), acc.begin() + i);
    }
    void from_fd(int fd) {
        char buf[65536];
        for (;;) { ssize_t n = read(fd, buf, sizeof buf); if (n <= 0) break; add(buf, (size_t)n); }
        eof = true;
    }
    int open_udp(int port) {
        int s = socket(AF_INET, SOCK_DGRAM, 0);
        int rcv = 4 << 20; setsockopt(s, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof rcv);
        int one = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = INADDR_ANY;
        if (bind(s, (sockaddr*)&a, sizeof a) < 0) { perror("UDP bind"); close(s); return -1; }
        return s;
    }
    void from_udp_socket(int s) {
        char buf[65536];
        for (;;) { ssize_t n = recv(s, buf, sizeof buf, 0); if (n > 0) add(buf, (size_t)n); }
    }
    /* Fill up to 'want' output slots. */
    void take(std::vector<char>& dst, size_t want = 32) {
        static const char nullhdr[4] = { 0x47, 0x1f, (char)0xff, 0x10 };
        const double W = 0.050;                 // lateness tolerated before the schedule slips
        const double EARLY = 2.0;               // head due this far ahead => timing jumped, re-align
        std::lock_guard<std::mutex> g(m);
        double r = rate_pps;
        if (reanchor) { c_set = false; reanchor = false; }
        for (size_t k = 0; k < want; k++, k_out++) {
            double T = k_out * slot_s;
            bool sent = false;
            if (!q.empty() && r > 0) {
                double s = stream_time(q.front().n, r);
                if (!c_set || s > T - c + EARLY) { if (c_set) resyncs++; c = T - s; c_set = true; }
                if (s <= T - c) {
                    if (s < T - c - W) { c = T - s; slips++; }
                    dst.insert(dst.end(), q.front().p.begin(), q.front().p.end());
                    q.pop_front(); real++; sent = true;
                }
            }
            if (!sent) {
                size_t o = dst.size();
                dst.resize(o + 188, (char)0xff);
                memcpy(&dst[o], nullhdr, 4);
                nulls++;
            }
        }
    }
    size_t queued() { std::lock_guard<std::mutex> g(m); return q.size(); }
};
