// ==============================================================================
// Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
// Module:        src/sdr_auditor.cpp
// Role:          10-Rule Deterministic SDR Receiver Posture Auditor Implementation
// Organization:  Logihertz Systems (OPC) Private Limited
// Developer:     Sameer Tawade (Creator, Founder, Director & Sole Developer)
// Contact:       ops@logihertz.com / security@logihertz.com
// License:       Apache-2.0
// ==============================================================================

#include "sdr_auditor.hpp"
#include <algorithm>

namespace logihertz {

const std::vector<RuleDefinition>& SdrAuditor::get_rules() {
    static const std::vector<RuleDefinition> s_rules = {
        {
            "LH-SDR-001",
            "Carson's Bandwidth / Nyquist Sampling Violation",
            Severity::Critical,
            "CWE-682",
            25,
            "Baseband sampling rate is less than 2*(delta_f_max + f_audio), causing irreversible aliasing distortion.",
            "Configure sample_rate_hz >= 2 * (freq_deviation_hz + max_audio_freq_hz) (minimum 240 kHz for WBFM)."
        },
        {
            "LH-SDR-002",
            "ADC Saturation / RF Overload Protection Disabled",
            Severity::Critical,
            "CWE-190",
            25,
            "ADC clipping guard is disabled or LNA front-end gain exceeds safe threshold (> 45 dB).",
            "Enable adc_clipping_guard_enabled: true and constrain lna_gain_db <= 45.0 dB to prevent intermodulation."
        },
        {
            "LH-SDR-003",
            "Unauthenticated Raw IQ Stream Exposed on Network",
            Severity::Critical,
            "CWE-345",
            25,
            "Raw unencrypted baseband IQ stream server binds to external non-loopback network interface.",
            "Bind IQ stream server strictly to 127.0.0.1 or enforce TLS client certificate mutual authentication."
        },
        {
            "LH-SDR-004",
            "Missing DC Offset Removal or IQ Imbalance Correction",
            Severity::High,
            "CWE-682",
            10,
            "DC blocker or Gram-Schmidt amplitude/phase imbalance compensation is disabled, creating central spike and mirror image.",
            "Set dc_offset_correction: true and iq_imbalance_correction: true across receiver conditioning pipeline."
        },
        {
            "LH-SDR-005",
            "Permissive or Disabled Carrier Squelch Gate",
            Severity::High,
            "CWE-400",
            10,
            "Carrier squelch gate is disabled or threshold is set too low (< -95 dBFS), outputting continuous acoustic noise.",
            "Enable squelch_enabled: true and calibrate squelch_threshold_dbfs between -80 dBFS and -60 dBFS."
        },
        {
            "LH-SDR-006",
            "Insufficient Anti-Aliasing Decimation Filter Order",
            Severity::High,
            "CWE-20",
            10,
            "FIR decimation filter taps is less than 31, allowing out-of-band RF noise to alias into audible audio.",
            "Increase anti_alias_filter_taps >= 31 using a symmetric Hamming-windowed sinc design."
        },
        {
            "LH-SDR-007",
            "Unbounded IQ Ring-Buffer Memory Allocation",
            Severity::High,
            "CWE-770",
            10,
            "DMA ring buffer bounds check disabled or buffer size exceeds safe limit (> 256 MB), creating OOM risk.",
            "Enable dma_ring_bounds_check: true and constrain max_iq_buffer_mb <= 256 MB."
        },
        {
            "LH-SDR-008",
            "Invalid or Missing WBFM De-Emphasis Time Constant",
            Severity::Medium,
            "CWE-682",
            4,
            "WBFM mode de-emphasis time constant is invalid (must be 50 us for India/EU or 75 us for Americas).",
            "Set deemphasis_tau_us to 50.0 (EU/India/ITU) or 75.0 (Americas) to prevent treble distortion."
        },
        {
            "LH-SDR-009",
            "Uncalibrated Local Oscillator (LO) Crystal Drift",
            Severity::Medium,
            "CWE-330",
            4,
            "PPM frequency error correction is disabled or maximum drift exceeds 2.5 PPM without TCXO reference.",
            "Enable ppm_correction_enabled: true and calibrate receiver crystal or utilize a TCXO/GPSDO reference."
        },
        {
            "LH-SDR-010",
            "Missing RF Preselector or Bias-Tee Overcurrent Guard",
            Severity::Low,
            "CWE-755",
            1,
            "Hardware RF band-pass preselector is disabled or active bias-tee lacks overcurrent auto-shutdown protection.",
            "Enable rf_preselector_enabled: true and ensure biastee_overcurrent_guard: true."
        }
    };
    return s_rules;
}

AuditReport SdrAuditor::audit_policy(const JsonValue& policy, const std::string& manifest_path) {
    AuditReport report;
    report.manifest_path = manifest_path;
    report.total_rules = 10;

    const auto& rules = get_rules();

    // 1. LH-SDR-001 (CRITICAL, 25)
    double sample_rate = policy.has_key("sample_rate_hz") ? policy.get("sample_rate_hz")->as_double(0.0) : 0.0;
    double freq_dev = policy.has_key("freq_deviation_hz") ? policy.get("freq_deviation_hz")->as_double(75000.0) : 75000.0;
    double max_audio = policy.has_key("max_audio_freq_hz") ? policy.get("max_audio_freq_hz")->as_double(15000.0) : 15000.0;
    double min_nyquist = 2.0 * (freq_dev + max_audio);
    if (sample_rate < min_nyquist) {
        report.findings.push_back({
            rules[0].id,
            rules[0].name,
            "CRITICAL",
            rules[0].weight,
            rules[0].cwe,
            "sample_rate_hz (" + std::to_string(sample_rate) + ") < Carson Nyquist minimum (" + std::to_string(min_nyquist) + ").",
            rules[0].remediation
        });
    }

    // 2. LH-SDR-002 (CRITICAL, 25)
    bool adc_guard = policy.has_key("adc_clipping_guard_enabled") ? policy.get("adc_clipping_guard_enabled")->as_bool(false) : false;
    double lna_gain = policy.has_key("lna_gain_db") ? policy.get("lna_gain_db")->as_double(50.0) : 50.0;
    if (!adc_guard || lna_gain > 45.0) {
        report.findings.push_back({
            rules[1].id,
            rules[1].name,
            "CRITICAL",
            rules[1].weight,
            rules[1].cwe,
            "adc_clipping_guard_enabled=" + std::string(adc_guard ? "true" : "false") + " or lna_gain_db=" + std::to_string(lna_gain) + " (> 45.0 dB).",
            rules[1].remediation
        });
    }

    // 3. LH-SDR-003 (CRITICAL, 25)
    bool net_server = policy.has_key("iq_network_server_enabled") ? policy.get("iq_network_server_enabled")->as_bool(false) : false;
    std::string bind_addr = policy.has_key("bind_address") ? policy.get("bind_address")->as_string("") : "";
    if (net_server && bind_addr != "127.0.0.1") {
        report.findings.push_back({
            rules[2].id,
            rules[2].name,
            "CRITICAL",
            rules[2].weight,
            rules[2].cwe,
            "iq_network_server_enabled is true with non-loopback bind_address: '" + bind_addr + "'.",
            rules[2].remediation
        });
    }

    // 4. LH-SDR-004 (HIGH, 10)
    bool dc_corr = policy.has_key("dc_offset_correction") ? policy.get("dc_offset_correction")->as_bool(false) : false;
    bool iq_corr = policy.has_key("iq_imbalance_correction") ? policy.get("iq_imbalance_correction")->as_bool(false) : false;
    if (!dc_corr || !iq_corr) {
        report.findings.push_back({
            rules[3].id,
            rules[3].name,
            "HIGH",
            rules[3].weight,
            rules[3].cwe,
            "dc_offset_correction=" + std::string(dc_corr ? "true" : "false") + ", iq_imbalance_correction=" + std::string(iq_corr ? "true" : "false") + ".",
            rules[3].remediation
        });
    }

    // 5. LH-SDR-005 (HIGH, 10)
    bool squelch = policy.has_key("squelch_enabled") ? policy.get("squelch_enabled")->as_bool(false) : false;
    double squelch_thresh = policy.has_key("squelch_threshold_dbfs") ? policy.get("squelch_threshold_dbfs")->as_double(-100.0) : -100.0;
    if (!squelch || squelch_thresh < -95.0) {
        report.findings.push_back({
            rules[4].id,
            rules[4].name,
            "HIGH",
            rules[4].weight,
            rules[4].cwe,
            "squelch_enabled=" + std::string(squelch ? "true" : "false") + ", squelch_threshold_dbfs=" + std::to_string(squelch_thresh) + " (< -95.0 dBFS).",
            rules[4].remediation
        });
    }

    // 6. LH-SDR-006 (HIGH, 10)
    int64_t taps = policy.has_key("anti_alias_filter_taps") ? policy.get("anti_alias_filter_taps")->as_int64(0) : 0;
    if (taps < 31) {
        report.findings.push_back({
            rules[5].id,
            rules[5].name,
            "HIGH",
            rules[5].weight,
            rules[5].cwe,
            "anti_alias_filter_taps=" + std::to_string(taps) + " (minimum 31 required).",
            rules[5].remediation
        });
    }

    // 7. LH-SDR-007 (HIGH, 10)
    int64_t max_buf = policy.has_key("max_iq_buffer_mb") ? policy.get("max_iq_buffer_mb")->as_int64(512) : 512;
    bool ring_check = policy.has_key("dma_ring_bounds_check") ? policy.get("dma_ring_bounds_check")->as_bool(false) : false;
    if (max_buf > 256 || !ring_check) {
        report.findings.push_back({
            rules[6].id,
            rules[6].name,
            "HIGH",
            rules[6].weight,
            rules[6].cwe,
            "max_iq_buffer_mb=" + std::to_string(max_buf) + " (> 256 MB) or dma_ring_bounds_check=" + std::string(ring_check ? "true" : "false") + ".",
            rules[6].remediation
        });
    }

    // 8. LH-SDR-008 (MEDIUM, 4)
    std::string mode = policy.has_key("mode") ? policy.get("mode")->as_string("WBFM") : "WBFM";
    double tau = policy.has_key("deemphasis_tau_us") ? policy.get("deemphasis_tau_us")->as_double(0.0) : 0.0;
    if (mode == "WBFM" && (std::abs(tau - 50.0) > 0.1 && std::abs(tau - 75.0) > 0.1)) {
        report.findings.push_back({
            rules[7].id,
            rules[7].name,
            "MEDIUM",
            rules[7].weight,
            rules[7].cwe,
            "mode is WBFM but deemphasis_tau_us is " + std::to_string(tau) + " (must be 50.0 or 75.0).",
            rules[7].remediation
        });
    }

    // 9. LH-SDR-009 (MEDIUM, 4)
    bool ppm_corr = policy.has_key("ppm_correction_enabled") ? policy.get("ppm_correction_enabled")->as_bool(false) : false;
    double lo_drift = policy.has_key("max_lo_drift_ppm") ? policy.get("max_lo_drift_ppm")->as_double(10.0) : 10.0;
    if (!ppm_corr || lo_drift > 2.5) {
        report.findings.push_back({
            rules[8].id,
            rules[8].name,
            "MEDIUM",
            rules[8].weight,
            rules[8].cwe,
            "ppm_correction_enabled=" + std::string(ppm_corr ? "true" : "false") + " or max_lo_drift_ppm=" + std::to_string(lo_drift) + " (> 2.5 PPM).",
            rules[8].remediation
        });
    }

    // 10. LH-SDR-010 (LOW, 1)
    bool preselector = policy.has_key("rf_preselector_enabled") ? policy.get("rf_preselector_enabled")->as_bool(false) : false;
    bool biastee = policy.has_key("biastee_overcurrent_guard") ? policy.get("biastee_overcurrent_guard")->as_bool(false) : false;
    if (!preselector || !biastee) {
        report.findings.push_back({
            rules[9].id,
            rules[9].name,
            "LOW",
            rules[9].weight,
            rules[9].cwe,
            "rf_preselector_enabled=" + std::string(preselector ? "true" : "false") + " or biastee_overcurrent_guard=" + std::string(biastee ? "true" : "false") + ".",
            rules[9].remediation
        });
    }

    uint32_t penalty = 0;
    for (const auto& f : report.findings) {
        if (f.severity == "CRITICAL") report.critical_count++;
        else if (f.severity == "HIGH") report.high_count++;
        else if (f.severity == "MEDIUM") report.medium_count++;
        else if (f.severity == "LOW") report.low_count++;
        penalty += f.weight;
    }

    report.score = (penalty >= 100) ? 0 : (100 - penalty);
    report.status = (report.score == 100) ? "PASSED" : "FAILED";
    report.passed_rules = report.total_rules - report.findings.size();

    return report;
}

} // namespace logihertz
