/* FFTW3 stand-in for gr::fft::fft_complex_rev (inverse complex FFT). */
#pragma once
#include <gnuradio/types.h>
#include <fftw3.h>
namespace gr { namespace fft {
class fft_complex_rev {
public:
    fft_complex_rev(int n, int = 1) : d_n(n) {
        d_in = fftwf_alloc_complex(n); d_out = fftwf_alloc_complex(n);
        d_plan = fftwf_plan_dft_1d(n, d_in, d_out, FFTW_BACKWARD, FFTW_ESTIMATE);
    }
    ~fft_complex_rev() { fftwf_destroy_plan(d_plan); fftwf_free(d_in); fftwf_free(d_out); }
    gr_complex* get_inbuf() const { return reinterpret_cast<gr_complex*>(d_in); }
    gr_complex* get_outbuf() const { return reinterpret_cast<gr_complex*>(d_out); }
    int inbuf_length() const { return d_n; }
    int outbuf_length() const { return d_n; }
    void execute() { fftwf_execute(d_plan); }
private:
    int d_n; fftwf_complex *d_in, *d_out; fftwf_plan d_plan;
};
}} // namespace gr::fft
