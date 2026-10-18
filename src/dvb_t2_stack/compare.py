#!/usr/bin/env python3
"""compare.py ref.cf32 test.cf32 - sample-by-sample IQ comparison."""
import sys, numpy as np
a = np.fromfile(sys.argv[1], dtype=np.complex64); b = np.fromfile(sys.argv[2], dtype=np.complex64)
n = min(len(a), len(b)); d = np.abs(a[:n] - b[:n])
print(f"lengths {len(a)} / {len(b)}   max difference {d.max():.3g}   samples > 1e-4: {(d > 1e-4).sum()}")
