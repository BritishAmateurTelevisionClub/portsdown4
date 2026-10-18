/* Plain-C++ versions of the two VOLK kernels used by the T2 blocks. */
#pragma once
#include <complex>
typedef std::complex<float> lv_32fc_t;
static inline void volk_32f_s32f_multiply_32f(float* c, const float* a, float s, unsigned int n)
{ for (unsigned int i = 0; i < n; i++) c[i] = a[i] * s; }
static inline void volk_32fc_x2_multiply_32fc(lv_32fc_t* c, const lv_32fc_t* a, const lv_32fc_t* b, unsigned int n)
{ for (unsigned int i = 0; i < n; i++) c[i] = a[i] * b[i]; }
