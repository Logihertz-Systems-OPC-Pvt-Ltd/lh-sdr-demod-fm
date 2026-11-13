# Contributing to `lh-sdr-demod-fm`

Thank you for your interest in contributing to **`lh-sdr-demod-fm`** (`bin/lh-sdr-fm`), part of the Logihertz 60-Tool Open-Source Ecosystem.

## Core Architectural Mandates

1. **Zero External Dependencies**:
   - The project strictly uses the **C++17 Standard Library** (`std`).
   - Do NOT introduce external dependencies (e.g. `liquid-dsp`, `fftw3`, `libsndfile`, `boost`, or `nlohmann/json`). All DSP math, WAV creation, and JSON parsing are implemented clean-room.
2. **Compiler Rigor**:
   - The codebase must compile cleanly under both `g++` and `clang++` with flags:
     `-std=c++17 -O3 -Wall -Wextra -Werror -pedantic`
3. **SIMD & Performance**:
   - Core DSP algorithms (conjugate delay product and rational atan2) support SIMD acceleration via `__AVX2__` on x86_64, `__ARM_NEON` on ARM64, and a bit-accurate scalar fallback.
   - All SIMD code paths must match the scalar reference within $< 10^{-3}$ absolute tolerance.
4. **RF & DSP Constraints**:
   - Demodulation logic must respect Carson's rule:
     $$B_T = 2(\Delta f_{\max} + f_{\text{audio}})$$
   - The baseband sample rate $f_s$ must satisfy the Nyquist criterion $f_s \ge B_T$.

## Local Development & Testing

```bash
# Clean previous builds
make clean

# Compile release binary and test suite with g++
make CXX=g++ all test

# Compile with clang++
make CXX=clang++ all test
```

## Submitting Contributions

1. Verify that all 10 security rules in `sdr_auditor.cpp` remain deterministic.
2. Ensure new features are accompanied by tests in `tests/test_runner.cpp`.
3. Submit cleanly signed commits with author attribution.
