# Logihertz SDR FM Demodulator (`lh-sdr-demod-fm`)

[![Organization](https://img.shields.io/badge/Organization-Logihertz%20Systems-000000.svg)](https://logihertz.com/)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache--2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![C++: 17](https://img.shields.io/badge/C%2B%2B-17-blue.svg?logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![Zero Dependencies](https://img.shields.io/badge/Dependencies-0%20External%20Libs-success.svg)](#architecture)
[![OASIS SARIF v2.1.0](https://img.shields.io/badge/OASIS-SARIF%20v2.1.0-green.svg)](https://docs.oasis-open.org/sarif/sarif/v2.1.0/sarif-v2.1.0.html)
[![CI Matrix](https://github.com/Logihertz-Systems-OPC-Pvt-Ltd/lh-sdr-demod-fm/actions/workflows/ci.yml/badge.svg)](https://github.com/Logihertz-Systems-OPC-Pvt-Ltd/lh-sdr-demod-fm/actions/workflows/ci.yml)

**Production-grade, zero-dependency C++17 SIMD-Accelerated Wideband & Narrowband FM Quadrature Demodulator, IQ Imbalance Compensator, IIR De-Emphasis Engine, and 10-Rule SDR Receiver Posture Auditor.**

---

## Executive Summary

Software-Defined Radio (SDR) front-ends (such as RTL-SDR, HackRF, BladeRF, USRP, and sovereign FPGA transceivers) stream raw in-phase and quadrature ($I/Q$) complex baseband samples. Practical hardware experiences DC offset leakage, analog quadrature amplitude imbalance, and phase skew. In mission-critical monitoring, uncompensated front-ends cause distortion, spurious intermodulation products, and audio degradation.

**`lh-sdr-demod-fm`** (`bin/lh-sdr-fm`) delivers an ultra-fast, clean-room C++17 digital signal processing engine:
- **100% Zero External Third-Party Library Dependencies**: Implemented strictly using the C++17 Standard Library (`std`).
- **SIMD Vectorized Discriminator**: AVX2 (`__AVX2__`) on x86_64 and ARM NEON (`__ARM_NEON`) on ARM64, alongside an exact scalar fallback.
- **IQ Conditioning Engine**: High-pass DC offset blocker, Gram-Schmidt orthogonalization for amplitude ($\epsilon$) and phase ($\phi$) compensation, RSSI power in dBFS, and ADC clipping detector.
- **Dual FM Demodulation**: Wideband FM (WBFM, $\Delta f_{\max} = \pm 75\text{ kHz}$) and Narrowband FM (NBFM, $\Delta f_{\max} = \pm 5\text{ kHz}$).
- **Anti-Aliasing FIR Decimator & IIR De-Emphasis**: Windowed-sinc low-pass FIR filter decimating from RF baseband ($f_s = 240\text{ kHz}$) to audio rate ($f_{\text{audio}} = 48\text{ kHz}$), with configurable $50\ \mu\text{s}$ (India/EU/ITU) and $75\ \mu\text{s}$ (Americas) de-emphasis.
- **Carrier Squelch Gate**: Smoothly mutes audio output when carrier RSSI falls below a calibrated dBFS threshold.
- **16-Bit RIFF/WAV Serializer**: Clamped 16-bit linear PCM audio file writer and synthetic FM modulated IQ signal generator.
- **10-Rule SDR Posture Auditor**: Audits receiver configurations against Carson's rule, buffer bounds, LO stability, and ADC clipping guards with ANSI table, JSON, and OASIS SARIF v2.1.0 output.

---

## Mathematical Architecture: FM Demodulation & Conditioning

### 1. Quadrature Polar Discriminator

For a continuous stream of complex baseband samples $z[n] = I[n] + j Q[n]$, the conjugate delay product $w[n] = z[n] \cdot z^*[n-1]$ represents the instantaneous complex phase increment:

$$\text{Re}(w[n]) = I[n]I[n-1] + Q[n]Q[n-1]$$

$$\text{Im}(w[n]) = Q[n]I[n-1] - I[n]Q[n-1]$$

$$\Delta\theta[n] = \text{atan2}\left(\text{Im}(w[n]), \; \text{Re}(w[n])\right)$$

The normalized demodulated baseband audio sample is given by scaling against sample rate $f_s$ and maximum frequency deviation $\Delta f_{\max}$:

$$y[n] = \frac{f_s}{2\pi \Delta f_{\max}} \Delta\theta[n]$$

### 2. Carson's Bandwidth Rule & Nyquist Sampling

According to Carson's rule, an FM transmission with peak frequency deviation $\Delta f_{\max}$ and modulating signal bandwidth $f_m$ occupies total transmission bandwidth $B_T$:

$$B_T = 2(\Delta f_{\max} + f_m)$$

The SDR receiver baseband sample rate $f_s$ must satisfy the complex Nyquist sampling criterion $f_s \ge B_T$. For broadcast WBFM ($\Delta f_{\max} = 75\text{ kHz}, f_m = 15\text{ kHz}$), $B_T = 180\text{ kHz}$; hence $f_s \ge 240\text{ kHz}$ is standard.

### 3. Gram-Schmidt IQ Imbalance Compensation

Given amplitude imbalance $\epsilon$ and phase skew angle $\phi$:

$$I_{\text{corr}}[n] = \tilde{I}[n], \qquad Q_{\text{corr}}[n] = \frac{\tilde{Q}[n] - (1 + \epsilon)\sin(\phi)\tilde{I}[n]}{(1 + \epsilon)\cos(\phi)}$$

### 4. Single-Pole IIR De-Emphasis Filter

FM transmitters apply pre-emphasis to boost high audio frequencies. The receiver restores flat frequency response using a single-pole IIR low-pass filter with time constant $\tau \in \{50\ \mu\text{s}, 75\ \mu\text{s}\}$:

$$\alpha = 1.0 - \exp\left(-\frac{1.0}{f_{\text{audio}} \cdot \tau}\right)$$

$$y_{\text{deemph}}[n] = \alpha \cdot y[n] + (1.0 - \alpha) \cdot y_{\text{deemph}}[n-1]$$

---

## Core Detection Rules (`LH-SDR-001` .. `LH-SDR-010`)

| Rule ID | Severity | Weight ($W_r$) | CWE | Trigger Condition & Description |
|:---|:---:|:---:|:---:|:---|
| **`LH-SDR-001`** | `CRITICAL` | 25 | CWE-682 | Carson's rule / Nyquist sampling violation: `sample_rate_hz < 2 * (freq_deviation_hz + max_audio_freq_hz)` |
| **`LH-SDR-002`** | `CRITICAL` | 25 | CWE-190 | ADC saturation protection disabled (`adc_clipping_guard_enabled: false` or `lna_gain_db > 45.0`) |
| **`LH-SDR-003`** | `CRITICAL` | 25 | CWE-345 | Unauthenticated raw IQ stream exposed on network (`iq_network_server_enabled: true` and `bind_address != "127.0.0.1"`) |
| **`LH-SDR-004`** | `HIGH` | 10 | CWE-682 | Missing DC offset removal or IQ imbalance correction (`dc_offset_correction: false` or `iq_imbalance_correction: false`) |
| **`LH-SDR-005`** | `HIGH` | 10 | CWE-400 | Carrier squelch gate disabled or threshold too permissive (`squelch_enabled: false` or `squelch_threshold_dbfs < -95.0`) |
| **`LH-SDR-006`** | `HIGH` | 10 | CWE-20 | Insufficient anti-aliasing FIR decimation filter taps (`anti_alias_filter_taps < 31`) prior to audio downsampling |
| **`LH-SDR-007`** | `HIGH` | 10 | CWE-770 | Unbounded IQ ring-buffer allocation (`max_iq_buffer_mb > 256` or `dma_ring_bounds_check: false`) |
| **`LH-SDR-008`** | `MEDIUM` | 4 | CWE-682 | Invalid or missing WBFM de-emphasis time constant (`mode == "WBFM"` and `deemphasis_tau_us` not in `[50, 75]`) |
| **`LH-SDR-009`** | `MEDIUM` | 4 | CWE-330 | Uncalibrated local oscillator crystal drift (`ppm_correction_enabled: false` or `max_lo_drift_ppm > 2.5`) |
| **`LH-SDR-010`** | `LOW` | 1 | CWE-755 | Missing hardware RF band-pass preselector or bias-tee guard (`rf_preselector_enabled: false` or `biastee_overcurrent_guard: false`) |

### Standardized Security Posture Score:

$$S_{\text{posture}} = \max\left(0, 100 - \sum_{r \in \text{Violations}} W_r\right) = \max\left(0, 100 - (25 \cdot c_{\text{CRIT}} + 10 \cdot c_{\text{HIGH}} + 4 \cdot c_{\text{MED}} + 1 \cdot c_{\text{LOW}})\right)$$

- **Hardened Receiver (`hardened_sdr_receiver.json`)**: $S_{\text{posture}} = 100/100$ (Exit code `0`).
- **Vulnerable Receiver (`vulnerable_sdr_receiver.json`)**: $S_{\text{posture}} = \max(0, 100 - 124) = 0/100$ (Exit code `1`).

---

## Build & Test Instructions

```bash
# Clean previous builds
make clean

# Compile release binary and run full test suite with g++
make CXX=g++ all test

# Compile and test with clang++
make CXX=clang++ all test
```

---

## CLI Reference (`bin/lh-sdr-fm`)

```bash
# 1. Audit SDR receiver policy manifest
bin/lh-sdr-fm scan fixtures/hardened_sdr_receiver.json --format table
bin/lh-sdr-fm scan fixtures/hardened_sdr_receiver.json --format sarif --output report.sarif

# 2. Synthesize clean FM modulated IQ test carrier
bin/lh-sdr-fm gen-iq /tmp/tone.iq --tone-hz 1000 --sample-rate 240000 --samples 240000

# 3. Demodulate IQ file into 16-bit RIFF/WAV audio
bin/lh-sdr-fm demod /tmp/tone.iq --output /tmp/audio.wav --mode WBFM --sample-rate 240000 --audio-rate 48000 --deemph 50

# 4. Benchmark SIMD vs Scalar throughput
bin/lh-sdr-fm benchmark --samples 1000000

# 5. Version & Help
bin/lh-sdr-fm --version
bin/lh-sdr-fm --help
```

---

## Exit Codes

| Exit Code | Semantic Meaning |
|:---:|:---|
| **`0`** | Clean scan (Score: 100, zero violations) or successful demodulation |
| **`1`** | Posture violation detected, carrier squelched, or signal corrupted |
| **`2`** | Invalid CLI usage, missing file, or malformed JSON syntax |

---

## Corporate Attribution & Governance

- **Organization**: [Logihertz Systems (OPC) Private Limited](https://logihertz.com/)
- **GitHub Organization**: [`Logihertz-Systems-OPC-Pvt-Ltd`](https://github.com/Logihertz-Systems-OPC-Pvt-Ltd)
- **Creator, Founder, Director & Sole Developer**: **Sameer Tawade** ([`ops@logihertz.com`](mailto:ops@logihertz.com) / [`security@logihertz.com`](mailto:security@logihertz.com))
- **Git Commit Author**: `logihertzsystems <ops@logihertz.com>`
- **License**: Released under the [Apache License, Version 2.0](https://www.apache.org/licenses/LICENSE-2.0).

Copyright &copy; 2026 Logihertz Systems (OPC) Private Limited. All Rights Reserved.
