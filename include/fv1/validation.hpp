#pragma once

#include <fv1/runtime.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fv1 {

struct ValidationAudio {
    std::uint32_t sample_rate{};
    std::vector<StereoFrame> frames;
};

struct ValidationConfig {
    // Maximum absolute time offset searched before comparing the recordings.
    // Positive reported delay means capture lags the reference.
    double max_alignment_ms{100.0};

    // If enabled, one common least-squares gain is applied to the capture for
    // residual/SNR measurements. Raw gain error is still reported separately.
    bool gain_match_residual{false};

    // Spectral comparison uses one Hann-windowed FFT from the aligned region.
    // Supported values are powers of two >= 256. If the aligned recording is
    // shorter, the largest usable power of two is selected automatically.
    std::size_t fft_size{16384};
    double spectral_floor_db{-90.0};

    // First-pass acceptance thresholds. They are intentionally configurable:
    // physical-board validation may use looser limits while bit-identical
    // regression/reference captures can demand much tighter limits.
    double minimum_correlation{0.995};
    double maximum_residual_rms_dbfs{-45.0};
    double maximum_residual_peak_dbfs{-24.0};
};

struct ValidationChannelMetrics {
    double reference_rms_dbfs{-200.0};
    double capture_rms_dbfs{-200.0};
    double gain_error_db{};
    double correlation{};
    double residual_rms_dbfs{-200.0};
    double residual_peak_dbfs{-200.0};
    double snr_db{200.0};
};

struct ValidationFrequencyPoint {
    double frequency_hz{};
    double magnitude_error_db{};
    double phase_error_degrees{};
    double reference_level_dbfs{-200.0};
};

struct ValidationResult {
    ValidationConfig config{};
    std::uint32_t sample_rate{};
    std::int64_t capture_delay_frames{};
    double capture_delay_ms{};
    std::size_t compared_frames{};
    double applied_capture_gain_db{};
    ValidationChannelMetrics left;
    ValidationChannelMetrics right;
    double spectral_rms_magnitude_error_db{};
    double spectral_worst_magnitude_error_db{};
    double spectral_worst_phase_error_degrees{};
    std::vector<ValidationFrequencyPoint> frequency_response;
    std::vector<StereoFrame> residual;
    bool passed{};
    std::vector<std::string> failures;
};

// Shared validation WAV boundary. The reader accepts mono/stereo PCM16/24/32,
// IEEE float32 and WAVE_FORMAT_EXTENSIBLE PCM/float files. Writer emits stereo
// float32 WAV so captures/reports remain lossless within the host pipeline.
bool load_validation_wav(const std::filesystem::path& path,
                         ValidationAudio& audio,
                         std::string* error = nullptr);
bool write_validation_wav(const std::filesystem::path& path,
                          const ValidationAudio& audio,
                          std::string* error = nullptr);

ValidationResult validate_recordings(const ValidationAudio& reference,
                                     const ValidationAudio& capture,
                                     const ValidationConfig& config = {});

// Export a complete validation bundle. prefix=/tmp/pitch-maw creates:
//   pitch-maw.json, pitch-maw.md, pitch-maw-frequency.csv, pitch-maw-residual.wav
bool write_validation_report_bundle(const std::filesystem::path& prefix,
                                    const ValidationResult& result,
                                    std::string* error = nullptr);

// Deterministic validation stimuli intended to be fed to both the emulator and
// physical hardware. kind: sine, sweep, white, pink, impulse, multitone.
bool generate_validation_stimulus(ValidationAudio& audio,
                                  std::uint32_t sample_rate,
                                  double seconds,
                                  const std::string& kind,
                                  double level = 0.25,
                                  double frequency_hz = 440.0,
                                  double sweep_end_hz = 16000.0,
                                  std::uint32_t seed = 0x465631u,
                                  std::string* error = nullptr);

} // namespace fv1
