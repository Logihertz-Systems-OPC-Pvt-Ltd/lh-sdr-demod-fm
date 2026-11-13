// ==============================================================================
// Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
// Module:        include/wav_writer.hpp
// Role:          16-Bit RIFF/WAV Serializer & Synthetic FM IQ Generator
// Organization:  Logihertz Systems (OPC) Private Limited
// Developer:     Sameer Tawade (Creator, Founder, Director & Sole Developer)
// Contact:       ops@logihertz.com / security@logihertz.com
// License:       Apache-2.0
// ==============================================================================

#ifndef LH_WAV_WRITER_HPP
#define LH_WAV_WRITER_HPP

#include "sdr_types.hpp"
#include <string>
#include <vector>

namespace logihertz {

class WavWriter {
public:
    /// Write mono float audio samples (-1.0 to +1.0) into 16-bit PCM RIFF WAV file
    static bool write_wav_16bit(
        const std::string& filepath,
        const std::vector<float>& audio_samples,
        uint32_t sample_rate_hz
    );

    /// Synthesize clean FM-modulated complex baseband IQ samples modulated with a sine tone
    static std::vector<IQSample> synthesize_fm_iq(
        double sample_rate_hz,
        double freq_dev_hz,
        double tone_freq_hz,
        size_t num_samples,
        float amplitude = 0.8f
    );

    /// Save IQ samples to binary interleaved float32 file
    static bool save_iq_file(
        const std::string& filepath,
        const std::vector<IQSample>& samples
    );

    /// Load IQ samples from binary interleaved float32 file
    static std::vector<IQSample> load_iq_file(const std::string& filepath);
};

} // namespace logihertz

#endif // LH_WAV_WRITER_HPP
