// ==============================================================================
// Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
// Module:        src/wav_writer.cpp
// Role:          16-Bit RIFF WAV Serializer & Synthetic FM Generator Implementation
// Organization:  Logihertz Systems (OPC) Private Limited
// Developer:     Sameer Tawade (Creator, Founder, Director & Sole Developer)
// Contact:       ops@logihertz.com / security@logihertz.com
// License:       Apache-2.0
// ==============================================================================

#include "wav_writer.hpp"

#include <cmath>
#include <fstream>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace logihertz {

bool WavWriter::write_wav_16bit(
    const std::string& filepath,
    const std::vector<float>& audio_samples,
    uint32_t sample_rate_hz
) {
    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    uint16_t num_channels = 1;
    uint16_t bits_per_sample = 16;
    uint32_t byte_rate = sample_rate_hz * num_channels * (bits_per_sample / 8);
    uint16_t block_align = num_channels * (bits_per_sample / 8);
    uint32_t data_bytes = static_cast<uint32_t>(audio_samples.size() * sizeof(int16_t));
    uint32_t chunk_size = 36 + data_bytes;
    uint32_t subchunk1_size = 16;
    uint16_t audio_format = 1; // PCM

    // 1. RIFF Chunk Descriptor
    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&chunk_size), 4);
    out.write("WAVE", 4);

    // 2. "fmt " Subchunk
    out.write("fmt ", 4);
    out.write(reinterpret_cast<const char*>(&subchunk1_size), 4);
    out.write(reinterpret_cast<const char*>(&audio_format), 2);
    out.write(reinterpret_cast<const char*>(&num_channels), 2);
    out.write(reinterpret_cast<const char*>(&sample_rate_hz), 4);
    out.write(reinterpret_cast<const char*>(&byte_rate), 4);
    out.write(reinterpret_cast<const char*>(&block_align), 2);
    out.write(reinterpret_cast<const char*>(&bits_per_sample), 2);

    // 3. "data" Subchunk
    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&data_bytes), 4);

    // 4. Sample Payload
    for (float sample : audio_samples) {
        float clamped = std::clamp(sample, -1.0f, 1.0f);
        int16_t pcm = static_cast<int16_t>(clamped * 32767.0f);
        out.write(reinterpret_cast<const char*>(&pcm), sizeof(int16_t));
    }

    return out.good();
}

std::vector<IQSample> WavWriter::synthesize_fm_iq(
    double sample_rate_hz,
    double freq_dev_hz,
    double tone_freq_hz,
    size_t num_samples,
    float amplitude
) {
    std::vector<IQSample> iq;
    iq.reserve(num_samples);

    double phase = 0.0;
    const double dt = 1.0 / sample_rate_hz;
    const double two_pi_dev = 2.0 * M_PI * freq_dev_hz;
    const double two_pi_tone = 2.0 * M_PI * tone_freq_hz;

    for (size_t n = 0; n < num_samples; ++n) {
        double t = n * dt;
        double m = std::sin(two_pi_tone * t);
        phase += two_pi_dev * m * dt;

        // Wrap phase into [-pi, pi] to avoid precision drift
        while (phase > M_PI) {
            phase -= 2.0 * M_PI;
        }
        while (phase < -M_PI) {
            phase += 2.0 * M_PI;
        }

        IQSample s;
        s.i = amplitude * static_cast<float>(std::cos(phase));
        s.q = amplitude * static_cast<float>(std::sin(phase));
        iq.push_back(s);
    }

    return iq;
}

bool WavWriter::save_iq_file(
    const std::string& filepath,
    const std::vector<IQSample>& samples
) {
    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    for (const auto& s : samples) {
        out.write(reinterpret_cast<const char*>(&s.i), sizeof(float));
        out.write(reinterpret_cast<const char*>(&s.q), sizeof(float));
    }

    return out.good();
}

std::vector<IQSample> WavWriter::load_iq_file(const std::string& filepath) {
    std::ifstream in(filepath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        return {};
    }

    std::streamsize file_size = in.tellg();
    in.seekg(0, std::ios::beg);

    size_t num_samples = static_cast<size_t>(file_size) / (2 * sizeof(float));
    std::vector<IQSample> samples(num_samples);

    for (size_t n = 0; n < num_samples; ++n) {
        in.read(reinterpret_cast<char*>(&samples[n].i), sizeof(float));
        in.read(reinterpret_cast<char*>(&samples[n].q), sizeof(float));
    }

    return samples;
}

} // namespace logihertz
