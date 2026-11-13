// ==============================================================================
// Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
// Module:        src/iq_conditioner.cpp
// Role:          IQ Conditioning, DC Blocker & Gram-Schmidt Imbalance Implementation
// Organization:  Logihertz Systems (OPC) Private Limited
// Developer:     Sameer Tawade (Creator, Founder, Director & Sole Developer)
// Contact:       ops@logihertz.com / security@logihertz.com
// License:       Apache-2.0
// ==============================================================================

#include "iq_conditioner.hpp"
#include <cmath>

namespace logihertz {

ConditionerStats IqConditioner::compute_stats(const std::vector<IQSample>& samples) {
    ConditionerStats stats;
    if (samples.empty()) {
        return stats;
    }

    stats.total_samples = samples.size();
    double sum_i = 0.0;
    double sum_q = 0.0;
    double sum_pwr = 0.0;
    size_t clipped_count = 0;
    const float clip_sq_thresh = 0.995f * 0.995f;

    for (const auto& s : samples) {
        sum_i += s.i;
        sum_q += s.q;
        float pwr = s.i * s.i + s.q * s.q;
        sum_pwr += pwr;
        if (pwr >= clip_sq_thresh) {
            clipped_count++;
        }
    }

    stats.dc_i = static_cast<float>(sum_i / samples.size());
    stats.dc_q = static_cast<float>(sum_q / samples.size());
    double mean_pwr = (sum_pwr / samples.size()) + 1e-12;
    stats.rssi_dbfs = 10.0 * std::log10(mean_pwr);
    stats.clipped_fraction = static_cast<double>(clipped_count) / static_cast<double>(samples.size());

    return stats;
}

void IqConditioner::remove_dc_offset(std::vector<IQSample>& samples, float& out_dc_i, float& out_dc_q) {
    if (samples.empty()) {
        out_dc_i = 0.0f;
        out_dc_q = 0.0f;
        return;
    }

    double sum_i = 0.0;
    double sum_q = 0.0;
    for (const auto& s : samples) {
        sum_i += s.i;
        sum_q += s.q;
    }

    out_dc_i = static_cast<float>(sum_i / samples.size());
    out_dc_q = static_cast<float>(sum_q / samples.size());

    for (auto& s : samples) {
        s.i -= out_dc_i;
        s.q -= out_dc_q;
    }
}

void IqConditioner::compensate_imbalance(
    std::vector<IQSample>& samples,
    float amplitude_skew,
    float phase_skew_rad
) {
    if (samples.empty()) {
        return;
    }

    if (std::abs(amplitude_skew) < 1e-7f && std::abs(phase_skew_rad) < 1e-7f) {
        return;
    }

    float cos_phi = std::cos(phase_skew_rad);
    float sin_phi = std::sin(phase_skew_rad);
    float denom = (1.0f + amplitude_skew) * cos_phi;

    if (std::abs(denom) < 1e-6f) {
        denom = (denom >= 0.0f) ? 1e-6f : -1e-6f;
    }

    float numerator_mult = (1.0f + amplitude_skew) * sin_phi;

    for (auto& s : samples) {
        float i_corr = s.i;
        float q_corr = (s.q - numerator_mult * s.i) / denom;
        s.i = i_corr;
        s.q = q_corr;
    }
}

ConditionerStats IqConditioner::process(std::vector<IQSample>& samples, const DemodConfig& config) {
    float est_dc_i = 0.0f;
    float est_dc_q = 0.0f;

    if (config.dc_block_enabled) {
        remove_dc_offset(samples, est_dc_i, est_dc_q);
    }

    if (config.iq_comp_enabled) {
        compensate_imbalance(samples, config.amplitude_skew, config.phase_skew_rad);
    }

    ConditionerStats stats = compute_stats(samples);
    if (config.dc_block_enabled) {
        stats.dc_i = est_dc_i;
        stats.dc_q = est_dc_q;
    }
    return stats;
}

} // namespace logihertz
