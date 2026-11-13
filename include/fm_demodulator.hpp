// ==============================================================================
// Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
// Module:        include/fm_demodulator.hpp
// Role:          SIMD Quadrature Polar Discriminator, FIR Decimator & IIR De-Emphasis
// Organization:  Logihertz Systems (OPC) Private Limited
// Developer:     Sameer Tawade (Creator, Founder, Director & Sole Developer)
// Contact:       ops@logihertz.com / security@logihertz.com
// License:       Apache-2.0
// ==============================================================================

#ifndef LH_FM_DEMODULATOR_HPP
#define LH_FM_DEMODULATOR_HPP

#include "sdr_types.hpp"
#include <vector>

namespace logihertz {

class FmDemodulator {
public:
    /// Fast minimax rational polynomial approximation for atan2(y, x)
    static float fast_atan2(float y, float x);

    /// Discriminator implementation: Scalar exact (using std::atan2f)
    static std::vector<float> discriminate_scalar_exact(
        const std::vector<IQSample>& iq,
        double sample_rate_hz,
        double freq_dev_hz
    );

    /// Discriminator implementation: Fast SIMD/scalar minimax polynomial
    static std::vector<float> discriminate_fast(
        const std::vector<IQSample>& iq,
        double sample_rate_hz,
        double freq_dev_hz
    );

    /// Generate symmetric Hamming-windowed sinc low-pass FIR filter taps
    static std::vector<float> design_fir_lowpass(
        double sample_rate_hz,
        double cutoff_hz,
        size_t num_taps
    );

    /// Apply FIR filtering and decimation down to target audio rate
    static std::vector<float> decimate_fir(
        const std::vector<float>& input,
        const std::vector<float>& taps,
        size_t decimation_factor
    );

    /// Apply single-pole IIR de-emphasis filter (50 us or 75 us)
    static void apply_deemphasis(
        std::vector<float>& audio,
        double audio_rate_hz,
        double tau_us
    );

    /// Complete end-to-end demodulation pipeline:
    /// Conditioning -> Discriminator -> FIR Decimator -> De-emphasis -> Squelch
    static std::vector<float> demodulate(
        const std::vector<IQSample>& iq,
        const DemodConfig& config,
        ConditionerStats* out_stats = nullptr
    );

    /// Return active SIMD architecture identifier ("AVX2", "ARM_NEON", or "Scalar")
    static const char* simd_architecture();
};

} // namespace logihertz

#endif // LH_FM_DEMODULATOR_HPP
