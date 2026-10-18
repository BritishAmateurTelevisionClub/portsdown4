/* Minimal stand-in for GNU Radio runtime types (t2mod, GPLv3). */
#pragma once
#include <complex>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#define __GR_ATTR_ALIGNED(x) __attribute__((aligned(x)))
typedef std::complex<float> gr_complex;
typedef std::complex<double> gr_complexd;
typedef std::vector<int> gr_vector_int;
typedef std::vector<const void*> gr_vector_const_void_star;
typedef std::vector<void*> gr_vector_void_star;
