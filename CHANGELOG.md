# Changelog

All notable changes to `lh-sdr-demod-fm` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-11-13

### Added
- **Core C++17 Signal Processing Architecture**: Initial production-grade release of `lh-sdr-demod-fm` (`bin/lh-sdr-fm`) with 100% zero external third-party library dependencies.
- **IQ Conditioning Engine** (`iq_conditioner.cpp`):
  - DC offset estimation and single-pole IIR blocker.
  - Gram-Schmidt orthogonalization for IQ amplitude imbalance ($\epsilon$) and phase skew ($\phi$) correction.
  - RSSI power telemetry in dBFS and ADC saturation/clipping fraction detector.
- **SIMD Quadrature Polar Discriminator** (`fm_demodulator.cpp`):
  - Wideband FM (WBFM, $\pm 75\text{ kHz}$) and Narrowband FM (NBFM, $\pm 5\text{ kHz}$) quadrature demodulation.
  - Minimax rational polynomial $\text{atan2}$ approximation with AVX2 (`__AVX2__`) and ARM NEON (`__ARM_NEON`) vectorization, alongside portable bit-accurate scalar fallback.
  - Windowed-sinc low-pass anti-aliasing FIR decimator to downsample RF baseband (e.g. $240\text{ kHz}$) to audio rate (e.g. $48\text{ kHz}$).
  - Single-pole IIR de-emphasis filter supporting $50\ \mu\text{s}$ (India/EU/ITU) and $75\ \mu\text{s}$ (Americas) standards.
  - Carrier squelch threshold gating with smooth audio muting.
- **16-Bit RIFF/WAV Serializer & FM IQ Generator** (`wav_writer.cpp`):
  - Serialization of 16-bit little-endian linear PCM RIFF WAV audio files.
  - Synthetic FM modulated baseband IQ signal generator with sinusoidal audio modulation.
- **10-Rule SDR Receiver Posture Auditor** (`sdr_auditor.cpp`):
  - Deterministic evaluation of rules `LH-SDR-001` through `LH-SDR-010` mapping to CWEs.
  - Standardized Logihertz Security Posture Score calculation:
    $$S_{\text{posture}} = \max\left(0, 100 - \left(25 \cdot c_{\text{CRIT}} + 10 \cdot c_{\text{HIGH}} + 4 \cdot c_{\text{MED}} + 1 \cdot c_{\text{LOW}}\right)\right)$$
- **Multi-Format Exporters** (`exporters.cpp`):
  - ANSI box table with posture score banner.
  - Canonical JSON exporter.
  - OASIS SARIF v2.1.0 schema exporter.
- **Unified CLI Tool** (`main.cpp`):
  - Subcommands: `scan`, `demod`, `gen-iq`, `benchmark`, `--version`, and `--help`.
- **Deterministic Test Suite** (`tests/test_runner.cpp`):
  - 85+ test assertions covering DC removal, Gram-Schmidt correction, tone recovery SNR $> 40\text{ dB}$, SIMD vs scalar equivalence, WAV headers, 10 auditor rules, and CLI subprocess exit codes.
