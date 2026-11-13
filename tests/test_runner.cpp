// ==============================================================================
// Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
// Module:        tests/test_runner.cpp
// Role:          Zero-Dependency C++17 Comprehensive Unit & Regression Test Suite
// Organization:  Logihertz Systems (OPC) Private Limited
// Developer:     Sameer Tawade (Creator, Founder, Director & Sole Developer)
// Contact:       ops@logihertz.com / security@logihertz.com
// License:       Apache-2.0
// ==============================================================================

#include "sdr_types.hpp"
#include "iq_conditioner.hpp"
#include "fm_demodulator.hpp"
#include "wav_writer.hpp"
#include "json_parser.hpp"
#include "sdr_auditor.hpp"
#include "exporters.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

uint32_t g_assertions_passed = 0;
uint32_t g_assertions_failed = 0;

#define LH_ASSERT(cond, msg) do { \
    if (cond) { \
        g_assertions_passed++; \
    } else { \
        g_assertions_failed++; \
        std::cerr << "  [ASSERTION FAILED] " << __FILE__ << ":" << __LINE__ << " -> " << msg << "\n"; \
    } \
} while (0)

#define LH_ASSERT_NEAR(a, b, eps, msg) do { \
    if (std::abs((a) - (b)) <= (eps)) { \
        g_assertions_passed++; \
    } else { \
        g_assertions_failed++; \
        std::cerr << "  [ASSERTION FAILED] " << __FILE__ << ":" << __LINE__ << " -> " << msg \
                  << " (val=" << (a) << ", expected=" << (b) << ", eps=" << (eps) << ")\n"; \
    } \
} while (0)

std::string read_file_string(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

// -----------------------------------------------------------------------------
// Test Suite 1: Fast Atan2 Accuracy & Symmetries
// -----------------------------------------------------------------------------
void test_fast_atan2() {
    std::cout << "[RUNNING] Test Suite 1: Fast Atan2 Minimax Polynomial Accuracy...\n";

    // Quadrant cardinal axes
    LH_ASSERT_NEAR(logihertz::FmDemodulator::fast_atan2(0.0f, 1.0f), 0.0f, 1e-4f, "atan2(0, 1) == 0");
    LH_ASSERT_NEAR(logihertz::FmDemodulator::fast_atan2(1.0f, 0.0f), static_cast<float>(M_PI_2), 1e-4f, "atan2(1, 0) == pi/2");
    LH_ASSERT_NEAR(logihertz::FmDemodulator::fast_atan2(-1.0f, 0.0f), -static_cast<float>(M_PI_2), 1e-4f, "atan2(-1, 0) == -pi/2");
    LH_ASSERT_NEAR(std::abs(logihertz::FmDemodulator::fast_atan2(0.0f, -1.0f)), static_cast<float>(M_PI), 1e-3f, "atan2(0, -1) == +/-pi");
    LH_ASSERT_NEAR(logihertz::FmDemodulator::fast_atan2(0.0f, 0.0f), 0.0f, 1e-5f, "atan2(0, 0) == 0");

    // Diagonal angles
    float pi_4 = static_cast<float>(M_PI / 4.0);
    LH_ASSERT_NEAR(logihertz::FmDemodulator::fast_atan2(1.0f, 1.0f), pi_4, 1e-3f, "atan2(1, 1) == pi/4");
    LH_ASSERT_NEAR(logihertz::FmDemodulator::fast_atan2(1.0f, -1.0f), 3.0f * pi_4, 1e-3f, "atan2(1, -1) == 3*pi/4");
    LH_ASSERT_NEAR(logihertz::FmDemodulator::fast_atan2(-1.0f, 1.0f), -pi_4, 1e-3f, "atan2(-1, 1) == -pi/4");
    LH_ASSERT_NEAR(logihertz::FmDemodulator::fast_atan2(-1.0f, -1.0f), -3.0f * pi_4, 1e-3f, "atan2(-1, -1) == -3*pi/4");

    // Circular grid accuracy test: error < 0.0003 radians
    float max_err = 0.0f;
    for (int deg = 0; deg < 360; deg += 3) {
        float rad = static_cast<float>(deg * M_PI / 180.0);
        float x = std::cos(rad);
        float y = std::sin(rad);

        float exact = std::atan2(y, x);
        float approx = logihertz::FmDemodulator::fast_atan2(y, x);
        float err = std::abs(exact - approx);
        if (err > max_err) max_err = err;
    }
    LH_ASSERT(max_err < 0.0003f, "Fast atan2 max circular grid error < 0.0003 rad");
}

// -----------------------------------------------------------------------------
// Test Suite 2: IQ Conditioner & Front-End Telemetry
// -----------------------------------------------------------------------------
void test_iq_conditioner() {
    std::cout << "[RUNNING] Test Suite 2: IQ Conditioner & Gram-Schmidt Orthogonalization...\n";

    // 1. DC Offset Removal
    std::vector<logihertz::IQSample> samples(1000);
    for (size_t i = 0; i < samples.size(); ++i) {
        float theta = static_cast<float>(2.0 * M_PI * i / 50.0);
        samples[i].i = 0.5f * std::cos(theta) + 0.15f; // DC offset +0.15
        samples[i].q = 0.5f * std::sin(theta) - 0.25f; // DC offset -0.25
    }

    logihertz::DemodConfig cfg;
    cfg.dc_block_enabled = true;
    cfg.iq_comp_enabled = false;

    logihertz::ConditionerStats stats = logihertz::IqConditioner::process(samples, cfg);
    LH_ASSERT_NEAR(stats.dc_i, 0.15f, 0.02f, "Estimated DC offset I channel");
    LH_ASSERT_NEAR(stats.dc_q, -0.25f, 0.02f, "Estimated DC offset Q channel");

    // Verify after subtraction DC is near 0
    float new_dc_i = 0.0f, new_dc_q = 0.0f;
    for (const auto& s : samples) {
        new_dc_i += s.i;
        new_dc_q += s.q;
    }
    new_dc_i /= samples.size();
    new_dc_q /= samples.size();
    LH_ASSERT_NEAR(new_dc_i, 0.0f, 1e-4f, "DC I removed to 0");
    LH_ASSERT_NEAR(new_dc_q, 0.0f, 1e-4f, "DC Q removed to 0");

    // 2. RSSI calculation on full-scale carrier
    std::vector<logihertz::IQSample> carrier(1000);
    for (size_t i = 0; i < carrier.size(); ++i) {
        float theta = static_cast<float>(2.0 * M_PI * i / 20.0);
        carrier[i].i = 0.7071f * std::cos(theta);
        carrier[i].q = 0.7071f * std::sin(theta);
    }
    logihertz::ConditionerStats carrier_stats = logihertz::IqConditioner::compute_stats(carrier);
    LH_ASSERT_NEAR(carrier_stats.rssi_dbfs, -3.01, 0.2, "Carrier power ~ -3 dBFS for 0.7071 amplitude");

    // 3. ADC Clipping fraction
    std::vector<logihertz::IQSample> clip_test(100);
    for (size_t i = 0; i < 90; ++i) {
        clip_test[i].i = 0.5f;
        clip_test[i].q = 0.5f;
    }
    for (size_t i = 90; i < 100; ++i) {
        clip_test[i].i = 0.998f; // Saturated sample
        clip_test[i].q = 0.2f;
    }
    logihertz::ConditionerStats clip_stats = logihertz::IqConditioner::compute_stats(clip_test);
    LH_ASSERT_NEAR(clip_stats.clipped_fraction, 0.10, 0.001, "Detected 10% clipped samples");

    // 4. Gram-Schmidt correction
    std::vector<logihertz::IQSample> skewed(500);
    cfg.iq_comp_enabled = true;
    cfg.amplitude_skew = 0.2f;   // 20% amplitude mismatch
    cfg.phase_skew_rad = 0.15f;  // Phase skew
    for (size_t i = 0; i < skewed.size(); ++i) {
        skewed[i].i = 0.5f;
        skewed[i].q = 0.5f;
    }
    logihertz::IqConditioner::process(skewed, cfg);
    LH_ASSERT(skewed.size() == 500, "Conditioner processed samples length preserved");
}

// -----------------------------------------------------------------------------
// Test Suite 3: FM Demodulator & DSP Pipeline
// -----------------------------------------------------------------------------
void test_fm_demodulator() {
    std::cout << "[RUNNING] Test Suite 3: FM Demodulator, FIR Decimator & Squelch...\n";

    // 1. Generate 1 kHz modulated test tone at 240 kHz sample rate with +/- 75 kHz dev
    const double fs = 240000.0;
    const double fdev = 75000.0;
    const double ftone = 1000.0;
    const size_t n_samples = 48000;

    auto iq = logihertz::WavWriter::synthesize_fm_iq(fs, fdev, ftone, n_samples);
    LH_ASSERT(iq.size() == n_samples, "Synthesized expected sample count");

    // 2. Discriminator fidelity (Fast vs Scalar)
    auto scalar_audio = logihertz::FmDemodulator::discriminate_scalar_exact(iq, fs, fdev);
    auto fast_audio = logihertz::FmDemodulator::discriminate_fast(iq, fs, fdev);
    LH_ASSERT(scalar_audio.size() == fast_audio.size(), "Discriminators produce equal sample counts");

    double diff_sum = 0.0;
    for (size_t i = 0; i < scalar_audio.size(); ++i) {
        diff_sum += std::abs(scalar_audio[i] - fast_audio[i]);
    }
    double avg_diff = diff_sum / scalar_audio.size();
    LH_ASSERT(avg_diff < 0.0005, "Fast vs scalar discriminator average error < 0.0005");

    // 3. FIR Filter design properties
    auto taps = logihertz::FmDemodulator::design_fir_lowpass(240000.0, 15000.0, 35);
    LH_ASSERT(taps.size() == 35, "FIR tap count is 35 (odd symmetric)");

    // Check linear phase symmetry: taps[i] == taps[N-1-i]
    for (size_t i = 0; i < taps.size() / 2; ++i) {
        LH_ASSERT_NEAR(taps[i], taps[taps.size() - 1 - i], 1e-6, "FIR symmetric tap linear phase");
    }

    // Check unity DC gain
    double tap_sum = 0.0;
    for (float t : taps) tap_sum += t;
    LH_ASSERT_NEAR(tap_sum, 1.0, 1e-5, "FIR unity DC gain sum == 1.0");

    // 4. FIR Decimation
    std::vector<float> chirp(1000, 1.0f);
    auto decimated = logihertz::FmDemodulator::decimate_fir(chirp, taps, 5);
    LH_ASSERT(decimated.size() == 200, "Decimation factor 5 produces 1/5th samples");

    // 5. De-emphasis filter behavior: attenuates high frequency more than low frequency
    std::vector<float> audio_low(1000), audio_high(1000);
    for (size_t i = 0; i < 1000; ++i) {
        audio_low[i] = std::cos(static_cast<float>(2.0 * M_PI * 500.0 * i / 48000.0));
        audio_high[i] = std::cos(static_cast<float>(2.0 * M_PI * 10000.0 * i / 48000.0));
    }
    logihertz::FmDemodulator::apply_deemphasis(audio_low, 48000.0, 50.0);
    logihertz::FmDemodulator::apply_deemphasis(audio_high, 48000.0, 50.0);

    float rms_low = 0.0f, rms_high = 0.0f;
    for (size_t i = 200; i < 1000; ++i) {
        rms_low += audio_low[i] * audio_low[i];
        rms_high += audio_high[i] * audio_high[i];
    }
    LH_ASSERT(rms_high < rms_low * 0.3f, "De-emphasis 50us attenuates 10kHz significantly more than 500Hz");

    // 6. Complete demodulation pipeline with squelch
    logihertz::DemodConfig cfg;
    cfg.sample_rate_hz = 240000.0;
    cfg.audio_rate_hz = 48000.0;
    cfg.freq_dev_hz = 75000.0;
    cfg.mode = logihertz::FmMode::WBFM;
    cfg.squelch_enabled = true;
    cfg.squelch_threshold_dbfs = -65.0;

    logihertz::ConditionerStats stats;
    auto demod_audio = logihertz::FmDemodulator::demodulate(iq, cfg, &stats);
    LH_ASSERT(!demod_audio.empty(), "Demodulated audio is non-empty");
    LH_ASSERT(stats.rssi_dbfs > -10.0, "Active carrier RSSI > -10 dBFS");

    // Squelch gating test: weak signal
    auto weak_iq = logihertz::WavWriter::synthesize_fm_iq(fs, fdev, ftone, 10000, 0.00001f);
    auto muted_audio = logihertz::FmDemodulator::demodulate(weak_iq, cfg, &stats);
    LH_ASSERT(stats.rssi_dbfs < -65.0, "Weak carrier RSSI below threshold");
    bool all_zero = true;
    for (float s : muted_audio) {
        if (s != 0.0f) { all_zero = false; break; }
    }
    LH_ASSERT(all_zero, "Squelch mutes output to exact zeros");
}

// -----------------------------------------------------------------------------
// Test Suite 4: 16-Bit RIFF/WAV Writer & IO
// -----------------------------------------------------------------------------
void test_wav_writer() {
    std::cout << "[RUNNING] Test Suite 4: 16-Bit PCM RIFF WAV Writer & IQ IO...\n";

    std::string test_wav = "/tmp/test_lh_unit.wav";
    std::string test_iq = "/tmp/test_lh_unit.iq";

    // 1. WAV Header Validation
    std::vector<float> audio_test = {0.0f, 0.5f, 1.0f, -0.5f, -1.0f, 1.5f, -1.5f}; // Includes clamping test
    bool ok_wav = logihertz::WavWriter::write_wav_16bit(test_wav, audio_test, 48000);
    LH_ASSERT(ok_wav, "Successfully wrote WAV file");

    std::ifstream wav_in(test_wav, std::ios::binary);
    LH_ASSERT(wav_in.is_open(), "Opened written WAV file");

    char chunk_id[5] = {0};
    wav_in.read(chunk_id, 4);
    LH_ASSERT(std::string(chunk_id) == "RIFF", "WAV starts with 'RIFF'");

    wav_in.seekg(8);
    char format[5] = {0};
    wav_in.read(format, 4);
    LH_ASSERT(std::string(format) == "WAVE", "WAV format is 'WAVE'");

    wav_in.seekg(20);
    uint16_t audio_format = 0;
    wav_in.read(reinterpret_cast<char*>(&audio_format), 2);
    LH_ASSERT(audio_format == 1, "Audio format is PCM (1)");

    uint16_t num_channels = 0;
    wav_in.read(reinterpret_cast<char*>(&num_channels), 2);
    LH_ASSERT(num_channels == 1, "Mono audio (1 channel)");

    uint32_t sample_rate = 0;
    wav_in.read(reinterpret_cast<char*>(&sample_rate), 4);
    LH_ASSERT(sample_rate == 48000, "Sample rate is 48000 Hz");

    wav_in.close();

    // 2. Binary IQ file round-trip fidelity
    std::vector<logihertz::IQSample> orig_iq = {
        {0.12345f, -0.98765f},
        {-0.55555f, 0.44444f},
        {1.0f, 0.0f}
    };
    bool ok_iq = logihertz::WavWriter::save_iq_file(test_iq, orig_iq);
    LH_ASSERT(ok_iq, "Successfully saved IQ file");

    auto loaded_iq = logihertz::WavWriter::load_iq_file(test_iq);
    LH_ASSERT(loaded_iq.size() == orig_iq.size(), "Loaded exact number of IQ samples");
    for (size_t i = 0; i < orig_iq.size(); ++i) {
        LH_ASSERT_NEAR(loaded_iq[i].i, orig_iq[i].i, 1e-7f, "IQ sample I bit-exact");
        LH_ASSERT_NEAR(loaded_iq[i].q, orig_iq[i].q, 1e-7f, "IQ sample Q bit-exact");
    }

    std::remove(test_wav.c_str());
    std::remove(test_iq.c_str());
}

// -----------------------------------------------------------------------------
// Test Suite 5: Clean-Room JSON Parser
// -----------------------------------------------------------------------------
void test_json_parser() {
    std::cout << "[RUNNING] Test Suite 5: Zero-Dependency JSON Parser...\n";

    std::string json_str = R"({
        "string_val": "logihertz_sdr",
        "num_val": 240000.5,
        "bool_true": true,
        "bool_false": false,
        "null_val": null,
        "arr_val": [10, 20, 30],
        "nested_obj": {
            "key": "value"
        }
    })";

    std::string err;
    auto val = logihertz::JsonValue::parse(json_str, &err);
    LH_ASSERT(err.empty(), "JSON parsed without errors");
    LH_ASSERT(val.is_object(), "Root is JSON object");
    LH_ASSERT(val.has_key("string_val"), "Contains 'string_val'");
    LH_ASSERT(val.get("string_val")->as_string() == "logihertz_sdr", "string_val parsed correctly");
    LH_ASSERT_NEAR(val.get("num_val")->as_double(), 240000.5, 1e-4, "num_val parsed correctly");
    LH_ASSERT(val.get("bool_true")->as_bool() == true, "bool_true parsed correctly");
    LH_ASSERT(val.get("bool_false")->as_bool() == false, "bool_false parsed correctly");
    LH_ASSERT(val.get("null_val")->is_null(), "null_val is null");

    const auto* arr = val.get("arr_val");
    LH_ASSERT(arr != nullptr && arr->is_array(), "arr_val is array");
    LH_ASSERT(arr->arr_val.size() == 3, "arr_val has 3 elements");
    LH_ASSERT(arr->arr_val[1].as_int64() == 20, "arr_val[1] == 20");

    const auto* nested = val.get("nested_obj");
    LH_ASSERT(nested != nullptr && nested->is_object(), "nested_obj is object");
    LH_ASSERT(nested->has_key("key"), "nested_obj has 'key'");
    LH_ASSERT(nested->get("key")->as_string() == "value", "nested_obj.key == 'value'");

    // Negative tests
    std::string malformed1 = "{\"unclosed_string: 123}";
    std::string err1;
    auto v1 = logihertz::JsonValue::parse(malformed1, &err1);
    LH_ASSERT(!err1.empty(), "Detected unclosed string error");

    std::string malformed2 = "[1, 2, 3,]";
    std::string err2;
    auto v2 = logihertz::JsonValue::parse(malformed2, &err2);
    LH_ASSERT(!err2.empty(), "Detected trailing comma / unexpected token error");
}

// -----------------------------------------------------------------------------
// Test Suite 6: SDR Posture Auditor & 10 Rules
// -----------------------------------------------------------------------------
void test_sdr_auditor() {
    std::cout << "[RUNNING] Test Suite 6: 10-Rule Deterministic SDR Posture Auditor...\n";

    // 1. Hardened receiver fixture audit
    std::string hardened_str = read_file_string("fixtures/hardened_sdr_receiver.json");
    LH_ASSERT(!hardened_str.empty(), "Read hardened fixture file");

    std::string err_h;
    auto hardened_json = logihertz::JsonValue::parse(hardened_str, &err_h);
    LH_ASSERT(err_h.empty(), "Parsed hardened fixture JSON");

    auto report_h = logihertz::SdrAuditor::audit_policy(hardened_json, "fixtures/hardened_sdr_receiver.json");
    LH_ASSERT(report_h.score == 100, "Hardened receiver scores 100/100");
    LH_ASSERT(report_h.status == "PASSED", "Hardened receiver status is PASSED");
    LH_ASSERT(report_h.findings.empty(), "Hardened receiver has 0 findings");
    LH_ASSERT(report_h.passed_rules == 10, "All 10 rules passed");

    // 2. Vulnerable receiver fixture audit
    std::string vuln_str = read_file_string("fixtures/vulnerable_sdr_receiver.json");
    LH_ASSERT(!vuln_str.empty(), "Read vulnerable fixture file");

    std::string err_v;
    auto vuln_json = logihertz::JsonValue::parse(vuln_str, &err_v);
    LH_ASSERT(err_v.empty(), "Parsed vulnerable fixture JSON");

    auto report_v = logihertz::SdrAuditor::audit_policy(vuln_json, "fixtures/vulnerable_sdr_receiver.json");
    LH_ASSERT(report_v.score == 0, "Vulnerable receiver scores 0/100");
    LH_ASSERT(report_v.status == "FAILED", "Vulnerable receiver status is FAILED");
    LH_ASSERT(report_v.findings.size() == 10, "Vulnerable receiver triggers all 10 findings");
    LH_ASSERT(report_v.critical_count == 3, "3 Critical findings (LH-SDR-001..003)");
    LH_ASSERT(report_v.high_count == 4, "4 High findings (LH-SDR-004..007)");
    LH_ASSERT(report_v.medium_count == 2, "2 Medium findings (LH-SDR-008..009)");
    LH_ASSERT(report_v.low_count == 1, "1 Low finding (LH-SDR-010)");

    // 3. Isolated Rule Checks
    auto rules = logihertz::SdrAuditor::get_rules();
    LH_ASSERT(rules.size() == 10, "Rule registry contains 10 rules");

    // Verify Rule 1: LH-SDR-001
    logihertz::JsonValue r1_policy = hardened_json;
    for (auto& kv : r1_policy.obj_val) {
        if (kv.first == "sample_rate_hz") kv.second = logihertz::JsonValue(100000.0);
    }
    auto r1_rep = logihertz::SdrAuditor::audit_policy(r1_policy);
    LH_ASSERT(r1_rep.findings.size() == 1 && r1_rep.findings[0].rule_id == "LH-SDR-001", "Rule 1 violation isolated");
    LH_ASSERT(r1_rep.score == 75, "Rule 1 deduction: 100 - 25 = 75");

    // Verify Rule 2: LH-SDR-002
    logihertz::JsonValue r2_policy = hardened_json;
    for (auto& kv : r2_policy.obj_val) {
        if (kv.first == "adc_clipping_guard_enabled") kv.second = logihertz::JsonValue(false);
    }
    auto r2_rep = logihertz::SdrAuditor::audit_policy(r2_policy);
    LH_ASSERT(r2_rep.findings.size() == 1 && r2_rep.findings[0].rule_id == "LH-SDR-002", "Rule 2 violation isolated");

    // Verify Rule 3: LH-SDR-003
    logihertz::JsonValue r3_policy = hardened_json;
    for (auto& kv : r3_policy.obj_val) {
        if (kv.first == "iq_network_server_enabled") kv.second = logihertz::JsonValue(true);
        if (kv.first == "bind_address") kv.second = logihertz::JsonValue("0.0.0.0");
    }
    auto r3_rep = logihertz::SdrAuditor::audit_policy(r3_policy);
    LH_ASSERT(r3_rep.findings.size() == 1 && r3_rep.findings[0].rule_id == "LH-SDR-003", "Rule 3 violation isolated");

    // Verify Rule 4: LH-SDR-004
    logihertz::JsonValue r4_policy = hardened_json;
    for (auto& kv : r4_policy.obj_val) {
        if (kv.first == "dc_offset_correction") kv.second = logihertz::JsonValue(false);
    }
    auto r4_rep = logihertz::SdrAuditor::audit_policy(r4_policy);
    LH_ASSERT(r4_rep.findings.size() == 1 && r4_rep.findings[0].rule_id == "LH-SDR-004", "Rule 4 violation isolated");

    // Verify Rule 5: LH-SDR-005
    logihertz::JsonValue r5_policy = hardened_json;
    for (auto& kv : r5_policy.obj_val) {
        if (kv.first == "squelch_enabled") kv.second = logihertz::JsonValue(false);
    }
    auto r5_rep = logihertz::SdrAuditor::audit_policy(r5_policy);
    LH_ASSERT(r5_rep.findings.size() == 1 && r5_rep.findings[0].rule_id == "LH-SDR-005", "Rule 5 violation isolated");

    // Verify Rule 6: LH-SDR-006
    logihertz::JsonValue r6_policy = hardened_json;
    for (auto& kv : r6_policy.obj_val) {
        if (kv.first == "anti_alias_filter_taps") kv.second = logihertz::JsonValue(25.0);
    }
    auto r6_rep = logihertz::SdrAuditor::audit_policy(r6_policy);
    LH_ASSERT(r6_rep.findings.size() == 1 && r6_rep.findings[0].rule_id == "LH-SDR-006", "Rule 6 violation isolated");

    // Verify Rule 7: LH-SDR-007
    logihertz::JsonValue r7_policy = hardened_json;
    for (auto& kv : r7_policy.obj_val) {
        if (kv.first == "max_iq_buffer_mb") kv.second = logihertz::JsonValue(512.0);
    }
    auto r7_rep = logihertz::SdrAuditor::audit_policy(r7_policy);
    LH_ASSERT(r7_rep.findings.size() == 1 && r7_rep.findings[0].rule_id == "LH-SDR-007", "Rule 7 violation isolated");

    // Verify Rule 8: LH-SDR-008
    logihertz::JsonValue r8_policy = hardened_json;
    for (auto& kv : r8_policy.obj_val) {
        if (kv.first == "deemphasis_tau_us") kv.second = logihertz::JsonValue(0.0);
    }
    auto r8_rep = logihertz::SdrAuditor::audit_policy(r8_policy);
    LH_ASSERT(r8_rep.findings.size() == 1 && r8_rep.findings[0].rule_id == "LH-SDR-008", "Rule 8 violation isolated");

    // Verify Rule 9: LH-SDR-009
    logihertz::JsonValue r9_policy = hardened_json;
    for (auto& kv : r9_policy.obj_val) {
        if (kv.first == "ppm_correction_enabled") kv.second = logihertz::JsonValue(false);
    }
    auto r9_rep = logihertz::SdrAuditor::audit_policy(r9_policy);
    LH_ASSERT(r9_rep.findings.size() == 1 && r9_rep.findings[0].rule_id == "LH-SDR-009", "Rule 9 violation isolated");

    // Verify Rule 10: LH-SDR-010
    logihertz::JsonValue r10_policy = hardened_json;
    for (auto& kv : r10_policy.obj_val) {
        if (kv.first == "rf_preselector_enabled") kv.second = logihertz::JsonValue(false);
    }
    auto r10_rep = logihertz::SdrAuditor::audit_policy(r10_policy);
    LH_ASSERT(r10_rep.findings.size() == 1 && r10_rep.findings[0].rule_id == "LH-SDR-010", "Rule 10 violation isolated");
}

// -----------------------------------------------------------------------------
// Test Suite 7: Table, JSON, and SARIF v2.1.0 Exporters
// -----------------------------------------------------------------------------
void test_exporters() {
    std::cout << "[RUNNING] Test Suite 7: Exporters (ANSI Table, JSON, SARIF v2.1.0)...\n";

    std::string vuln_str = read_file_string("fixtures/vulnerable_sdr_receiver.json");
    auto vuln_json = logihertz::JsonValue::parse(vuln_str);
    auto report = logihertz::SdrAuditor::audit_policy(vuln_json, "fixtures/vulnerable_sdr_receiver.json");

    // Table export validation
    std::string table = logihertz::Exporters::format_table(report);
    LH_ASSERT(table.find("Logihertz SDR Receiver Posture Auditor") != std::string::npos, "Table contains header");
    LH_ASSERT(table.find("LH-SDR-001") != std::string::npos, "Table contains Rule 1 ID");
    LH_ASSERT(table.find("0/100") != std::string::npos, "Table displays score 0/100");

    // JSON export validation
    std::string json_rep = logihertz::Exporters::format_json(report);
    std::string err_j;
    auto rep_val = logihertz::JsonValue::parse(json_rep, &err_j);
    LH_ASSERT(err_j.empty(), "Report JSON parses cleanly");
    LH_ASSERT(rep_val.get("score")->as_int64() == 0, "Report JSON score is 0");
    LH_ASSERT(rep_val.get("status")->as_string() == "FAILED", "Report JSON status is FAILED");

    // SARIF export validation
    std::string sarif = logihertz::Exporters::format_sarif(report);
    std::string err_s;
    auto sarif_val = logihertz::JsonValue::parse(sarif, &err_s);
    LH_ASSERT(err_s.empty(), "Report SARIF parses cleanly");
    LH_ASSERT(sarif_val.get("version")->as_string() == "2.1.0", "SARIF version 2.1.0");

    const auto* runs = sarif_val.get("runs");
    LH_ASSERT(runs != nullptr && runs->is_array() && !runs->arr_val.empty(), "SARIF contains runs");
    const auto& driver = *runs->arr_val[0].get("tool")->get("driver");
    LH_ASSERT(driver.get("name")->as_string() == "lh-sdr-fm", "SARIF driver name is 'lh-sdr-fm'");
    LH_ASSERT(driver.get("version")->as_string() == "0.1.0", "SARIF driver version is '0.1.0'");
    const auto* sarif_rules = driver.get("rules");
    LH_ASSERT(sarif_rules != nullptr && sarif_rules->arr_val.size() == 10, "SARIF contains 10 rules");
}

// -----------------------------------------------------------------------------
// Test Suite 8: CLI Subprocess Smoke Execution
// -----------------------------------------------------------------------------
void test_cli_smoke() {
    std::cout << "[RUNNING] Test Suite 8: CLI Subprocess Smoke Tests...\n";

    // 1. Version command
    int code_v = std::system("bin/lh-sdr-fm --version > /dev/null 2>&1");
    LH_ASSERT(code_v == 0, "CLI --version exits with 0");

    // 2. Help command
    int code_h = std::system("bin/lh-sdr-fm --help > /dev/null 2>&1");
    LH_ASSERT(code_h == 0, "CLI --help exits with 0");

    // 3. Scan hardened fixture -> exit 0
    int code_sh = std::system("bin/lh-sdr-fm scan fixtures/hardened_sdr_receiver.json > /dev/null 2>&1");
    LH_ASSERT(code_sh == 0, "CLI scan hardened fixture exits with 0 (score 100)");

    // 4. Scan vulnerable fixture -> exit 1
    int code_sv = std::system("bin/lh-sdr-fm scan fixtures/vulnerable_sdr_receiver.json > /dev/null 2>&1");
    LH_ASSERT(WEXITSTATUS(code_sv) == 1, "CLI scan vulnerable fixture exits with 1 (score < 100)");

    // 5. Invalid command -> exit 2
    int code_inv = std::system("bin/lh-sdr-fm invalid-cmd > /dev/null 2>&1");
    LH_ASSERT(WEXITSTATUS(code_inv) == 2, "CLI invalid command exits with 2");

    // 6. Missing scan file -> exit 2
    int code_mis = std::system("bin/lh-sdr-fm scan nonexistent.json > /dev/null 2>&1");
    LH_ASSERT(WEXITSTATUS(code_mis) == 2, "CLI scan nonexistent file exits with 2");

    // 7. Demodulate fixture sample_1khz_tone.iq -> exit 0
    int code_demod = std::system("bin/lh-sdr-fm demod fixtures/sample_1khz_tone.iq --output /tmp/cli_test.wav > /dev/null 2>&1");
    LH_ASSERT(code_demod == 0, "CLI demod fixture tone exits with 0");
    std::remove("/tmp/cli_test.wav");
}

} // namespace

int main() {
    std::cout << "==============================================================================\n";
    std::cout << "  Logihertz SDR Demodulator & Posture Auditor Test Suite (lh-sdr-demod-fm)\n";
    std::cout << "==============================================================================\n";

    test_fast_atan2();
    test_iq_conditioner();
    test_fm_demodulator();
    test_wav_writer();
    test_json_parser();
    test_sdr_auditor();
    test_exporters();
    test_cli_smoke();

    std::cout << "==============================================================================\n";
    std::cout << "  Summary: " << g_assertions_passed << " passed, " << g_assertions_failed << " failed.\n";
    std::cout << "==============================================================================\n";

    if (g_assertions_failed > 0) {
        std::cerr << "TEST SUITE FAILED with " << g_assertions_failed << " failure(s)!\n";
        return 1;
    }

    std::cout << "ALL TESTS PASSED SUCCESSFULLY!\n";
    return 0;
}
