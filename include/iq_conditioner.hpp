// ==============================================================================
// Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
// Module:        include/iq_conditioner.hpp
// Role:          IQ Conditioning, DC Blocker & Gram-Schmidt Imbalance Correction
// Organization:  Logihertz Systems (OPC) Private Limited
// Developer:     Sameer Tawade (Creator, Founder, Director & Sole Developer)
// Contact:       ops@logihertz.com / security@logihertz.com
// License:       Apache-2.0
// ==============================================================================

#ifndef LH_IQ_CONDITIONER_HPP
#define LH_IQ_CONDITIONER_HPP

#include "sdr_types.hpp"
#include <vector>

namespace logihertz {

class IqConditioner {
public:
    /// Calculate carrier power (RSSI in dBFS), DC bias, and ADC saturation stats
    static ConditionerStats compute_stats(const std::vector<IQSample>& samples);

    /// Compute and subtract mean DC offset from I and Q channels
    static void remove_dc_offset(std::vector<IQSample>& samples, float& out_dc_i, float& out_dc_q);

    /// Apply Gram-Schmidt orthogonalization for amplitude and phase skew correction
    static void compensate_imbalance(std::vector<IQSample>& samples, float amplitude_skew, float phase_skew_rad);

    /// Execute complete conditioning pipeline based on DemodConfig
    static ConditionerStats process(std::vector<IQSample>& samples, const DemodConfig& config);
};

} // namespace logihertz

#endif // LH_IQ_CONDITIONER_HPP
