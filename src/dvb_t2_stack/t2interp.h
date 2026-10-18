// t2interp.h - software polyphase interpolator for the DVB-T2 Lime path.
//
// The T2 modulator runs at only ~1.14 MS/s for a 1 MHz channel, so the first
// spectral image of the signal sits just 180 kHz outside its edge. The Lime's
// own interpolation does not remove it. This block interpolates by L with a
// Kaiser-windowed FIR, moving the images well clear of the signal.
#ifndef T2INTERP_H
#define T2INTERP_H
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

class T2Interp {
public:
    explicit T2Interp(int L) : L_(L < 1 ? 1 : L), tpp_(0) {
        if (L_ < 2) return;
        const double att = 55.0;                 // stopband attenuation, dB
        const double dF = 0.12 / L_;             // transition width: pass to 0.44*fs_in, stop from 0.56*fs_in
        int n = (int)std::ceil((att - 7.95) / (14.36 * dF));
        n = ((n + L_ - 1) / L_) * L_;            // whole number of taps per phase
        tpp_ = n / L_;
        const double beta = 0.1102 * (att - 8.7);
        std::vector<double> h(n);
        double sum = 0;
        for (int i = 0; i < n; i++) {
            double x = (i - (n - 1) / 2.0) / L_;
            double s = (std::fabs(x) < 1e-12) ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
            double r = 2.0 * i / (n - 1) - 1.0;
            double w = i0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / i0(beta);
            h[i] = s * w;
            sum += h[i];
        }
        taps_.assign((size_t)L_ * tpp_, 0.0f);   // phase-major, reversed in time
        for (int p = 0; p < L_; p++)
            for (int j = 0; j < tpp_; j++)
                taps_[(size_t)p * tpp_ + j] = (float)(h[(size_t)(tpp_ - 1 - j) * L_ + p] * L_ / sum);
        tr_.assign(tpp_ - 1, 0.0f);
        ti_.assign(tpp_ - 1, 0.0f);
    }

    int factor() const { return L_; }
    int taps_per_phase() const { return tpp_; }
    // Impulse response h[0..L*tpp-1], for testing
    std::vector<float> impulse_response() const {
        std::vector<float> h((size_t)L_ * tpp_);
        for (int p = 0; p < L_; p++)
            for (int j = 0; j < tpp_; j++)
                h[(size_t)(tpp_ - 1 - j) * L_ + p] = taps_[(size_t)p * tpp_ + j];
        return h;
    }

    // Interpolate n input samples into n*L output samples
    template <typename C>
    void process(const C* in, size_t n, std::vector<C>& out) {
        out.resize(n * L_);
        if (L_ < 2) { for (size_t i = 0; i < n; i++) out[i] = in[i]; return; }
        const size_t hl = tpp_ - 1;
        tr_.resize(hl + n);
        ti_.resize(hl + n);
        for (size_t i = 0; i < n; i++) { tr_[hl + i] = in[i].real(); ti_[hl + i] = in[i].imag(); }
        for (size_t i = 0; i < n; i++) {
            const float* xr = &tr_[i];
            const float* xi = &ti_[i];
            for (int p = 0; p < L_; p++) {
                const float* t = &taps_[(size_t)p * tpp_];
                float re = 0, im = 0;
                for (int j = 0; j < tpp_; j++) { re += t[j] * xr[j]; im += t[j] * xi[j]; }
                out[i * L_ + p] = C(re, im);
            }
        }
        // keep the last tpp-1 inputs for the next call
        std::copy(tr_.end() - hl, tr_.end(), tr_.begin());
        std::copy(ti_.end() - hl, ti_.end(), ti_.begin());
        tr_.resize(hl);
        ti_.resize(hl);
    }

private:
    static double i0(double x) {
        double t = 1, s = 1, q = x * x / 4;
        for (int k = 1; k < 60 && t > 1e-14 * s; k++) { t *= q / ((double)k * k); s += t; }
        return s;
    }
    int L_, tpp_;
    std::vector<float> taps_, tr_, ti_;
};
#endif
