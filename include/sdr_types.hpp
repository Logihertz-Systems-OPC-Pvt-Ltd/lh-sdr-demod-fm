// ==============================================================================
// Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
// Module:        include/sdr_types.hpp
// Role:          Core IQ Data Types, Demodulator Configuration & Auditor Models
// Organization:  Logihertz Systems (OPC) Private Limited
// Developer:     Sameer Tawade (Creator, Founder, Director & Sole Developer)
// Contact:       ops@logihertz.com / security@logihertz.com
// License:       Apache-2.0
// ==============================================================================

#ifndef LH_SDR_TYPES_HPP
#define LH_SDR_TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace logihertz {

/// Complex in-phase and quadrature baseband sample
struct IQSample {
    float i{0.0f};
    float q{0.0f};
};

/// Frequency Modulation operational mode
enum class FmMode {
    WBFM,  ///< Wideband FM (Broadcast, +/-75 kHz deviation)
    NBFM   ///< Narrowband FM (Communications, +/-5 kHz deviation)
};

/// Digital signal processing and demodulation configuration
struct DemodConfig {
    double sample_rate_hz{240000.0};   ///< Baseband IQ sampling rate (Hz)
    double audio_rate_hz{48000.0};     ///< Output audio sampling rate (Hz)
    double freq_dev_hz{75000.0};       ///< Peak frequency deviation (Hz)
    FmMode mode{FmMode::WBFM};         ///< FM demodulator mode
    double deemph_tau_us{50.0};        ///< De-emphasis time constant in microseconds (50 or 75)
    bool squelch_enabled{true};        ///< Carrier squelch gating enabled
    double squelch_threshold_dbfs{-65.0}; ///< Squelch mute threshold in dBFS
    bool dc_block_enabled{true};       ///< DC offset subtraction enabled
    bool iq_comp_enabled{true};        ///< Gram-Schmidt IQ imbalance correction enabled
    float amplitude_skew{0.0f};        ///< Amplitude imbalance epsilon
    float phase_skew_rad{0.0f};        ///< Phase skew phi in radians
};

/// IQ conditioning and receiver front-end telemetry statistics
struct ConditionerStats {
    double rssi_dbfs{-100.0};          ///< Carrier power relative to full-scale (dBFS)
    float dc_i{0.0f};                  ///< Estimated DC offset on I channel
    float dc_q{0.0f};                  ///< Estimated DC offset on Q channel
    double clipped_fraction{0.0};      ///< Ratio of saturated ADC samples (magnitude >= 0.995)
    size_t total_samples{0};           ///< Total processed samples count
};

/// Security classification severity
enum class Severity {
    Critical,
    High,
    Medium,
    Low
};

inline const char* severity_to_string(Severity s) {
    switch (s) {
        case Severity::Critical: return "CRITICAL";
        case Severity::High:     return "HIGH";
        case Severity::Medium:   return "MEDIUM";
        case Severity::Low:      return "LOW";
    }
    return "UNKNOWN";
}

inline uint32_t severity_weight(Severity s) {
    switch (s) {
        case Severity::Critical: return 25;
        case Severity::High:     return 10;
        case Severity::Medium:   return 4;
        case Severity::Low:      return 1;
    }
    return 0;
}

/// Normative security rule specification
struct RuleDefinition {
    std::string id;
    std::string name;
    Severity severity;
    std::string cwe;
    uint32_t weight;
    std::string description;
    std::string remediation;
};

/// Individual detected posture violation
struct Finding {
    std::string rule_id;
    std::string rule_name;
    std::string severity;
    uint32_t weight{0};
    std::string cwe;
    std::string message;
    std::string remediation;
};

/// Comprehensive SDR receiver posture evaluation report
struct AuditReport {
    std::string manifest_path;
    uint32_t score{100};
    std::string status{"PASSED"};
    size_t total_rules{10};
    size_t passed_rules{10};
    size_t critical_count{0};
    size_t high_count{0};
    size_t medium_count{0};
    size_t low_count{0};
    std::vector<Finding> findings;
};

} // namespace logihertz

#endif // LH_SDR_TYPES_HPP
