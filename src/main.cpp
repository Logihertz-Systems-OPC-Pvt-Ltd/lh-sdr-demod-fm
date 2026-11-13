// ==============================================================================
// Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
// Module:        src/main.cpp
// Role:          CLI Application Entrypoint & Subcommand Router
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

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

void print_version() {
    std::cout << "lh-sdr-fm v0.1.0 (Logihertz Systems (OPC) Private Limited)\n";
    std::cout << "Clean-room C++17 SIMD FM Demodulator & SDR Posture Auditor\n";
    std::cout << "Author: Sameer Tawade <ops@logihertz.com>\n";
    std::cout << "SIMD Acceleration: " << logihertz::FmDemodulator::simd_architecture() << "\n";
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  scan <policy.json>         Evaluate receiver policy against 10 posture rules\n";
    std::cout << "      [--format table|json|sarif] Output report format (default: table)\n";
    std::cout << "      [--output <file>]           Write output to destination file\n\n";
    std::cout << "  demod <input.iq>           Demodulate float32 IQ stream to 16-bit PCM WAV\n";
    std::cout << "      --output <file.wav>         Destination audio WAV file (required)\n";
    std::cout << "      [--mode WBFM|NBFM]          Demodulation mode (default: WBFM)\n";
    std::cout << "      [--sample-rate <Hz>]        Baseband sampling rate (default: 240000)\n";
    std::cout << "      [--audio-rate <Hz>]         Output audio rate (default: 48000)\n";
    std::cout << "      [--deemph <50|75>]          De-emphasis time constant in us (default: 50)\n";
    std::cout << "      [--squelch-threshold <dB>]  Squelch threshold in dBFS (default: -65.0)\n\n";
    std::cout << "  gen-iq <output.iq>         Synthesize FM modulated complex IQ test stream\n";
    std::cout << "      [--tone-hz <Hz>]            Modulating tone frequency (default: 1000)\n";
    std::cout << "      [--sample-rate <Hz>]        Baseband sampling rate (default: 240000)\n";
    std::cout << "      [--samples <count>]         Total sample count to synthesize (default: 240000)\n";
    std::cout << "      [--freq-dev <Hz>]           Frequency deviation (default: 75000)\n\n";
    std::cout << "  benchmark                  Benchmark SIMD vs scalar discriminator throughput\n";
    std::cout << "      [--samples <count>]         Sample count for benchmark (default: 1000000)\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help                 Display this help message and exit\n";
    std::cout << "  -v, --version              Display version information and exit\n";
}

std::string read_file_string(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int handle_scan(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Error: 'scan' requires a path to receiver policy JSON file.\n";
        return 2;
    }

    std::string policy_file = argv[2];
    std::string format = "table";
    std::string output_path = "";

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--format" && i + 1 < argc) {
            format = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else {
            std::cerr << "Error: Unknown argument for scan: " << arg << "\n";
            return 2;
        }
    }

    if (format != "table" && format != "json" && format != "sarif") {
        std::cerr << "Error: Invalid format '" << format << "'. Supported: table, json, sarif.\n";
        return 2;
    }

    std::string content = read_file_string(policy_file);
    if (content.empty()) {
        std::cerr << "Error: Failed to read policy file: " << policy_file << "\n";
        return 2;
    }

    std::string parse_err;
    logihertz::JsonValue policy = logihertz::JsonValue::parse(content, &parse_err);
    if (!parse_err.empty() || !policy.is_object()) {
        std::cerr << "Error: Failed to parse policy JSON (" << parse_err << "): " << policy_file << "\n";
        return 2;
    }

    logihertz::AuditReport report = logihertz::SdrAuditor::audit_policy(policy, policy_file);

    std::string rendered;
    if (format == "table") {
        rendered = logihertz::Exporters::format_table(report);
    } else if (format == "json") {
        rendered = logihertz::Exporters::format_json(report);
    } else if (format == "sarif") {
        rendered = logihertz::Exporters::format_sarif(report);
    }

    if (!output_path.empty()) {
        std::ofstream out_file(output_path);
        if (!out_file.is_open()) {
            std::cerr << "Error: Failed to write to output file: " << output_path << "\n";
            return 2;
        }
        out_file << rendered;
    } else {
        std::cout << rendered;
    }

    // Exit 0 if all 10 rules passed (Score: 100), exit 1 if any rule violated (Score < 100)
    return (report.score == 100) ? 0 : 1;
}

int handle_demod(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Error: 'demod' requires path to input float32 IQ file.\n";
        return 2;
    }

    std::string input_iq = argv[2];
    std::string output_wav = "";
    logihertz::DemodConfig config;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) {
            output_wav = argv[++i];
        } else if (arg == "--mode" && i + 1 < argc) {
            std::string m = argv[++i];
            if (m == "WBFM") {
                config.mode = logihertz::FmMode::WBFM;
                config.freq_dev_hz = 75000.0;
            } else if (m == "NBFM") {
                config.mode = logihertz::FmMode::NBFM;
                config.freq_dev_hz = 5000.0;
            } else {
                std::cerr << "Error: Invalid mode '" << m << "'. Must be WBFM or NBFM.\n";
                return 2;
            }
        } else if (arg == "--sample-rate" && i + 1 < argc) {
            config.sample_rate_hz = std::stod(argv[++i]);
        } else if (arg == "--audio-rate" && i + 1 < argc) {
            config.audio_rate_hz = std::stod(argv[++i]);
        } else if (arg == "--deemph" && i + 1 < argc) {
            config.deemph_tau_us = std::stod(argv[++i]);
        } else if (arg == "--squelch-threshold" && i + 1 < argc) {
            config.squelch_threshold_dbfs = std::stod(argv[++i]);
        } else {
            std::cerr << "Error: Unknown argument for demod: " << arg << "\n";
            return 2;
        }
    }

    if (output_wav.empty()) {
        std::cerr << "Error: 'demod' requires --output <file.wav>.\n";
        return 2;
    }

    std::vector<logihertz::IQSample> iq = logihertz::WavWriter::load_iq_file(input_iq);
    if (iq.empty()) {
        std::cerr << "Error: Failed to load IQ samples from file or file is empty: " << input_iq << "\n";
        return 2;
    }

    logihertz::ConditionerStats stats;
    std::vector<float> audio = logihertz::FmDemodulator::demodulate(iq, config, &stats);

    bool squelched = (config.squelch_enabled && stats.rssi_dbfs < config.squelch_threshold_dbfs);

    std::cout << "[INFO] Baseband IQ Samples: " << iq.size() << "\n";
    std::cout << "[INFO] Carrier RSSI:        " << std::fixed << std::setprecision(2) << stats.rssi_dbfs << " dBFS\n";
    std::cout << "[INFO] Estimated DC:        I=" << stats.dc_i << ", Q=" << stats.dc_q << "\n";
    std::cout << "[INFO] ADC Clipping:        " << (stats.clipped_fraction * 100.0) << " %\n";
    std::cout << "[INFO] Audio Output:        " << audio.size() << " samples (" << config.audio_rate_hz << " Hz)\n";
    std::cout << "[INFO] Squelch State:       " << (squelched ? "MUTED (Carrier below threshold)" : "ACTIVE") << "\n";

    if (!logihertz::WavWriter::write_wav_16bit(output_wav, audio, static_cast<uint32_t>(config.audio_rate_hz))) {
        std::cerr << "Error: Failed to write output WAV file: " << output_wav << "\n";
        return 2;
    }

    std::cout << "[SUCCESS] Audio written to " << output_wav << "\n";
    return squelched ? 1 : 0;
}

int handle_gen_iq(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Error: 'gen-iq' requires path to output IQ file.\n";
        return 2;
    }

    std::string output_file = argv[2];
    double tone_hz = 1000.0;
    double sample_rate_hz = 240000.0;
    size_t num_samples = 240000;
    double freq_dev_hz = 75000.0;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--tone-hz" && i + 1 < argc) {
            tone_hz = std::stod(argv[++i]);
        } else if (arg == "--sample-rate" && i + 1 < argc) {
            sample_rate_hz = std::stod(argv[++i]);
        } else if (arg == "--samples" && i + 1 < argc) {
            num_samples = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--freq-dev" && i + 1 < argc) {
            freq_dev_hz = std::stod(argv[++i]);
        } else {
            std::cerr << "Error: Unknown argument for gen-iq: " << arg << "\n";
            return 2;
        }
    }

    auto samples = logihertz::WavWriter::synthesize_fm_iq(
        sample_rate_hz,
        freq_dev_hz,
        tone_hz,
        num_samples
    );

    if (!logihertz::WavWriter::save_iq_file(output_file, samples)) {
        std::cerr << "Error: Failed to save IQ file: " << output_file << "\n";
        return 2;
    }

    std::cout << "[SUCCESS] Synthesized " << samples.size() << " FM IQ samples to " << output_file << "\n";
    std::cout << "  Tone:        " << tone_hz << " Hz\n";
    std::cout << "  Deviation:   " << freq_dev_hz << " Hz\n";
    std::cout << "  Sample Rate: " << sample_rate_hz << " Hz\n";
    return 0;
}

int handle_benchmark(int argc, char* argv[]) {
    size_t num_samples = 1000000;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--samples" && i + 1 < argc) {
            num_samples = static_cast<size_t>(std::stoul(argv[++i]));
        } else {
            std::cerr << "Error: Unknown argument for benchmark: " << arg << "\n";
            return 2;
        }
    }

    std::cout << "==============================================================================\n";
    std::cout << "  Logihertz FM Demodulator SIMD Discriminator Benchmark\n";
    std::cout << "==============================================================================\n";
    std::cout << "  Active SIMD Architecture: " << logihertz::FmDemodulator::simd_architecture() << "\n";
    std::cout << "  Synthesizing " << num_samples << " baseband IQ samples (240 kHz, +/-75 kHz dev)...\n";

    auto iq = logihertz::WavWriter::synthesize_fm_iq(240000.0, 75000.0, 1000.0, num_samples);

    // Warm-up
    auto warm1 = logihertz::FmDemodulator::discriminate_scalar_exact(iq, 240000.0, 75000.0);
    auto warm2 = logihertz::FmDemodulator::discriminate_fast(iq, 240000.0, 75000.0);

    // Time Scalar
    auto t0 = std::chrono::high_resolution_clock::now();
    auto scalar_out = logihertz::FmDemodulator::discriminate_scalar_exact(iq, 240000.0, 75000.0);
    auto t1 = std::chrono::high_resolution_clock::now();
    double scalar_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double scalar_msps = (static_cast<double>(num_samples) / 1e6) / (scalar_ms / 1000.0);

    // Time Fast / SIMD
    auto t2 = std::chrono::high_resolution_clock::now();
    auto fast_out = logihertz::FmDemodulator::discriminate_fast(iq, 240000.0, 75000.0);
    auto t3 = std::chrono::high_resolution_clock::now();
    double fast_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();
    double fast_msps = (static_cast<double>(num_samples) / 1e6) / (fast_ms / 1000.0);

    // Cross-Correlation and Error Metrics
    double max_err = 0.0;
    double sum_err = 0.0;
    double sum_s = 0.0, sum_f = 0.0;
    double sum_s2 = 0.0, sum_f2 = 0.0, sum_sf = 0.0;
    size_t count = scalar_out.size();

    for (size_t i = 0; i < count; ++i) {
        double s = scalar_out[i];
        double f = fast_out[i];
        double diff = std::abs(s - f);
        if (diff > max_err) max_err = diff;
        sum_err += diff;

        sum_s += s;
        sum_f += f;
        sum_s2 += s * s;
        sum_f2 += f * f;
        sum_sf += s * f;
    }

    double mean_err = sum_err / count;
    double num_corr = count * sum_sf - sum_s * sum_f;
    double den_corr = std::sqrt((count * sum_s2 - sum_s * sum_s) * (count * sum_f2 - sum_f * sum_f));
    double pearson_r = (den_corr > 1e-12) ? (num_corr / den_corr) : 1.0;

    // Signal-to-Noise Ratio (SNR) in dB
    double signal_power = sum_s2 / count;
    double noise_power = 0.0;
    for (size_t i = 0; i < count; ++i) {
        double d = scalar_out[i] - fast_out[i];
        noise_power += d * d;
    }
    noise_power /= count;
    double snr_db = (noise_power > 1e-15) ? (10.0 * std::log10(signal_power / noise_power)) : 99.9;

    std::cout << "\nResults:\n";
    std::cout << "  Scalar Discriminator:  " << std::fixed << std::setprecision(2)
              << scalar_ms << " ms (" << scalar_msps << " MSPS)\n";
    std::cout << "  Fast/SIMD Discriminator:" << std::fixed << std::setprecision(2)
              << fast_ms << " ms (" << fast_msps << " MSPS)\n";
    std::cout << "  Speedup:               " << std::fixed << std::setprecision(2)
              << (scalar_ms / fast_ms) << "x\n";
    std::cout << "\nFidelity Metrics:\n";
    std::cout << "  Maximum Absolute Error: " << std::scientific << std::setprecision(4) << max_err << "\n";
    std::cout << "  Mean Absolute Error:    " << std::scientific << std::setprecision(4) << mean_err << "\n";
    std::cout << "  Pearson Correlation r:  " << std::fixed << std::setprecision(6) << pearson_r << " (target >= 0.999)\n";
    std::cout << "  Calculated SNR:         " << std::fixed << std::setprecision(2) << snr_db << " dB (target >= 40 dB)\n";
    std::cout << "  Validation:             " << (pearson_r >= 0.999 && snr_db >= 40.0 ? "PASSED" : "FAILED") << "\n";
    std::cout << "==============================================================================\n";

    return (pearson_r >= 0.999) ? 0 : 1;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage("lh-sdr-fm");
        return 2;
    }

    std::string cmd = argv[1];

    if (cmd == "-h" || cmd == "--help" || cmd == "help") {
        print_usage("lh-sdr-fm");
        return 0;
    }

    if (cmd == "-v" || cmd == "--version" || cmd == "version") {
        print_version();
        return 0;
    }

    if (cmd == "scan") {
        return handle_scan(argc, argv);
    }

    if (cmd == "demod") {
        return handle_demod(argc, argv);
    }

    if (cmd == "gen-iq") {
        return handle_gen_iq(argc, argv);
    }

    if (cmd == "benchmark") {
        return handle_benchmark(argc, argv);
    }

    std::cerr << "Error: Unknown command '" << cmd << "'.\n";
    print_usage("lh-sdr-fm");
    return 2;
}
