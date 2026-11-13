// ==============================================================================
// Product:       Logihertz SDR FM Demodulator (lh-sdr-demod-fm)
// Module:        src/fm_demodulator.cpp
// Role:          SIMD Polar Discriminator, FIR Decimator & IIR De-Emphasis Implementation
// Organization:  Logihertz Systems (OPC) Private Limited
// Developer:     Sameer Tawade (Creator, Founder, Director & Sole Developer)
// Contact:       ops@logihertz.com / security@logihertz.com
// License:       Apache-2.0
// ==============================================================================

#include "fm_demodulator.hpp"
#include "iq_conditioner.hpp"

#include <cmath>
#include <algorithm>

#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace logihertz {

const char* FmDemodulator::simd_architecture() {
#if defined(__AVX2__)
    return "AVX2 (x86_64 256-bit)";
#elif defined(__ARM_NEON)
    return "ARM NEON (AArch64 128-bit)";
#else
    return "Scalar Portable C++17";
#endif
}

float FmDemodulator::fast_atan2(float y, float x) {
    if (x == 0.0f && y == 0.0f) {
        return 0.0f;
    }
    float ax = std::abs(x);
    float ay = std::abs(y);
    float mn = std::min(ax, ay);
    float mx = std::max(ax, ay);
    float a = mn / (mx + 1e-12f);
    float s = a * a;
    // Minimax polynomial approximation for atan(a) on [0, 1]
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) {
        r = 1.57079632679f - r;
    }
    if (x < 0.0f) {
        r = 3.14159265359f - r;
    }
    if (y < 0.0f) {
        r = -r;
    }
    return r;
}

std::vector<float> FmDemodulator::discriminate_scalar_exact(
    const std::vector<IQSample>& iq,
    double sample_rate_hz,
    double freq_dev_hz
) {
    if (iq.size() < 2) {
        return {};
    }

    std::vector<float> output;
    output.reserve(iq.size() - 1);

    const double norm_factor = sample_rate_hz / (2.0 * M_PI * freq_dev_hz);

    for (size_t n = 1; n < iq.size(); ++n) {
        // w[n] = z[n] * conj(z[n-1])
        float re = iq[n].i * iq[n - 1].i + iq[n].q * iq[n - 1].q;
        float im = iq[n].q * iq[n - 1].i - iq[n].i * iq[n - 1].q;

        float delta_theta = std::atan2(im, re);
        float audio_val = static_cast<float>(delta_theta * norm_factor);
        output.push_back(audio_val);
    }

    return output;
}

std::vector<float> FmDemodulator::discriminate_fast(
    const std::vector<IQSample>& iq,
    double sample_rate_hz,
    double freq_dev_hz
) {
    if (iq.size() < 2) {
        return {};
    }

    const size_t num_output = iq.size() - 1;
    std::vector<float> output(num_output);
    const float norm_factor = static_cast<float>(sample_rate_hz / (2.0 * M_PI * freq_dev_hz));

    size_t n = 1;

#if defined(__AVX2__)
    // Vectorized 8-wide AVX2 batch processing
    const __m256 v_norm = _mm256_set1_ps(norm_factor);
    const __m256 v_pi_2 = _mm256_set1_ps(1.57079632679f);
    const __m256 v_pi   = _mm256_set1_ps(3.14159265359f);
    const __m256 v_c1   = _mm256_set1_ps(-0.0464964749f);
    const __m256 v_c2   = _mm256_set1_ps(0.15931422f);
    const __m256 v_c3   = _mm256_set1_ps(-0.327622764f);
    const __m256 v_eps  = _mm256_set1_ps(1e-12f);
    const __m256 v_sign_mask = _mm256_set1_ps(-0.0f);

    while (n + 8 <= iq.size()) {
        float re_arr[8];
        float im_arr[8];
        for (int k = 0; k < 8; ++k) {
            size_t idx = n + k;
            re_arr[k] = iq[idx].i * iq[idx - 1].i + iq[idx].q * iq[idx - 1].q;
            im_arr[k] = iq[idx].q * iq[idx - 1].i - iq[idx].i * iq[idx - 1].q;
        }

        __m256 vx = _mm256_loadu_ps(re_arr);
        __m256 vy = _mm256_loadu_ps(im_arr);

        __m256 v_ax = _mm256_andnot_ps(v_sign_mask, vx);
        __m256 v_ay = _mm256_andnot_ps(v_sign_mask, vy);

        __m256 v_mn = _mm256_min_ps(v_ax, v_ay);
        __m256 v_mx = _mm256_max_ps(v_ax, v_ay);

        __m256 va = _mm256_div_ps(v_mn, _mm256_add_ps(v_mx, v_eps));
        __m256 vs = _mm256_mul_ps(va, va);

        // Polynomial: ((-0.0464964749 * s + 0.15931422) * s - 0.327622764) * s * a + a
        __m256 vr = _mm256_add_ps(_mm256_mul_ps(v_c1, vs), v_c2);
        vr = _mm256_add_ps(_mm256_mul_ps(vr, vs), v_c3);
        vr = _mm256_add_ps(_mm256_mul_ps(_mm256_mul_ps(vr, vs), va), va);

        // if (ay > ax) r = pi/2 - r
        __m256 v_swap_mask = _mm256_cmp_ps(v_ay, v_ax, _CMP_GT_OQ);
        __m256 v_r_swap = _mm256_sub_ps(v_pi_2, vr);
        vr = _mm256_blendv_ps(vr, v_r_swap, v_swap_mask);

        // if (x < 0) r = pi - r
        __m256 v_xneg_mask = _mm256_cmp_ps(vx, _mm256_setzero_ps(), _CMP_LT_OQ);
        __m256 v_r_xneg = _mm256_sub_ps(v_pi, vr);
        vr = _mm256_blendv_ps(vr, v_r_xneg, v_xneg_mask);

        // if (y < 0) r = -r
        __m256 v_yneg_mask = _mm256_cmp_ps(vy, _mm256_setzero_ps(), _CMP_LT_OQ);
        __m256 v_r_yneg = _mm256_sub_ps(_mm256_setzero_ps(), vr);
        vr = _mm256_blendv_ps(vr, v_r_yneg, v_yneg_mask);

        __m256 v_out = _mm256_mul_ps(vr, v_norm);
        _mm256_storeu_ps(&output[n - 1], v_out);

        n += 8;
    }
#endif

    // Scalar fallback for remainder
    for (; n < iq.size(); ++n) {
        float re = iq[n].i * iq[n - 1].i + iq[n].q * iq[n - 1].q;
        float im = iq[n].q * iq[n - 1].i - iq[n].i * iq[n - 1].q;

        float delta_theta = fast_atan2(im, re);
        output[n - 1] = delta_theta * norm_factor;
    }

    return output;
}

std::vector<float> FmDemodulator::design_fir_lowpass(
    double sample_rate_hz,
    double cutoff_hz,
    size_t num_taps
) {
    if (num_taps % 2 == 0) {
        num_taps++; // Ensure odd tap count for symmetric linear phase
    }

    std::vector<float> taps(num_taps);
    int m = static_cast<int>(num_taps - 1) / 2;
    double fc_norm = cutoff_hz / sample_rate_hz;
    double sum = 0.0;

    for (int i = 0; i < static_cast<int>(num_taps); ++i) {
        int n = i - m;
        double sinc = (n == 0)
            ? (2.0 * fc_norm)
            : std::sin(2.0 * M_PI * fc_norm * n) / (M_PI * n);
        double hamming = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (num_taps - 1));
        taps[i] = static_cast<float>(sinc * hamming);
        sum += taps[i];
    }

    if (std::abs(sum) > 1e-9) {
        for (auto& t : taps) {
            t = static_cast<float>(t / sum);
        }
    }

    return taps;
}

std::vector<float> FmDemodulator::decimate_fir(
    const std::vector<float>& input,
    const std::vector<float>& taps,
    size_t decimation_factor
) {
    if (input.empty() || taps.empty() || decimation_factor == 0) {
        return {};
    }

    const size_t num_taps = taps.size();
    const size_t output_len = input.size() / decimation_factor;
    std::vector<float> output;
    output.reserve(output_len);

    for (size_t out_idx = 0; out_idx < output_len; ++out_idx) {
        size_t in_center = out_idx * decimation_factor;
        float acc = 0.0f;

        for (size_t k = 0; k < num_taps; ++k) {
            int64_t in_pos = static_cast<int64_t>(in_center) - static_cast<int64_t>(k);
            if (in_pos >= 0 && in_pos < static_cast<int64_t>(input.size())) {
                acc += taps[k] * input[static_cast<size_t>(in_pos)];
            }
        }
        output.push_back(acc);
    }

    return output;
}

void FmDemodulator::apply_deemphasis(
    std::vector<float>& audio,
    double audio_rate_hz,
    double tau_us
) {
    if (audio.empty() || tau_us <= 0.0 || audio_rate_hz <= 0.0) {
        return;
    }

    const double tau_sec = tau_us * 1e-6;
    const float alpha = static_cast<float>(1.0 - std::exp(-1.0 / (audio_rate_hz * tau_sec)));
    const float one_minus_alpha = 1.0f - alpha;

    float prev_y = 0.0f;
    for (auto& sample : audio) {
        float y = alpha * sample + one_minus_alpha * prev_y;
        prev_y = y;
        sample = y;
    }
}

std::vector<float> FmDemodulator::demodulate(
    const std::vector<IQSample>& iq,
    const DemodConfig& config,
    ConditionerStats* out_stats
) {
    if (iq.size() < 2) {
        return {};
    }

    // Step 1: IQ Conditioning
    std::vector<IQSample> conditioned = iq;
    ConditionerStats stats = IqConditioner::process(conditioned, config);
    if (out_stats != nullptr) {
        *out_stats = stats;
    }

    // Step 2: Squelch check
    if (config.squelch_enabled && stats.rssi_dbfs < config.squelch_threshold_dbfs) {
        size_t dec_factor = static_cast<size_t>(std::max(1.0, std::round(config.sample_rate_hz / config.audio_rate_hz)));
        size_t audio_len = (iq.size() - 1) / dec_factor;
        return std::vector<float>(audio_len, 0.0f);
    }

    // Step 3: Quadrature Polar Discriminator
    std::vector<float> disc_out = discriminate_fast(
        conditioned,
        config.sample_rate_hz,
        config.freq_dev_hz
    );

    // Step 4: Low-pass anti-aliasing FIR Decimator
    size_t dec_factor = static_cast<size_t>(std::max(1.0, std::round(config.sample_rate_hz / config.audio_rate_hz)));
    double cutoff_hz = (config.mode == FmMode::WBFM) ? 15000.0 : 3000.0;
    std::vector<float> fir_taps = design_fir_lowpass(config.sample_rate_hz, cutoff_hz, 35);
    std::vector<float> audio = decimate_fir(disc_out, fir_taps, dec_factor);

    // Step 5: IIR De-emphasis
    if (config.mode == FmMode::WBFM) {
        apply_deemphasis(audio, config.audio_rate_hz, config.deemph_tau_us);
    }

    return audio;
}

} // namespace logihertz
