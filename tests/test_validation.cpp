#include <fv1/validation.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <string>

namespace fs = std::filesystem;

namespace {
int failures = 0;
void check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

void wr16(std::ostream& o, std::uint16_t v) {
    const char b[2]{static_cast<char>(v), static_cast<char>(v >> 8)}; o.write(b, 2);
}
void wr32(std::ostream& o, std::uint32_t v) {
    const char b[4]{static_cast<char>(v), static_cast<char>(v >> 8),
                    static_cast<char>(v >> 16), static_cast<char>(v >> 24)}; o.write(b, 4);
}
void make_extensible_pcm16_wav(const fs::path& path, std::uint32_t rate) {
    constexpr std::size_t frames = 128;
    constexpr std::uint16_t channels = 2;
    constexpr std::uint16_t bits = 16;
    constexpr std::uint32_t fmt_size = 40;
    const std::uint32_t data_size = static_cast<std::uint32_t>(frames * channels * (bits / 8));
    std::ofstream o(path, std::ios::binary);
    o.write("RIFF", 4); wr32(o, 4 + 8 + fmt_size + 8 + data_size); o.write("WAVE", 4);
    o.write("fmt ", 4); wr32(o, fmt_size);
    wr16(o, 0xfffe); wr16(o, channels); wr32(o, rate);
    wr32(o, rate * channels * 2u); wr16(o, channels * 2u); wr16(o, bits);
    wr16(o, 22); wr16(o, bits); wr32(o, 0x00000003u);
    const std::uint8_t guid[16]{
        0x01,0x00,0x00,0x00, 0x00,0x00,0x10,0x00,
        0x80,0x00,0x00,0xaa, 0x00,0x38,0x9b,0x71};
    o.write(reinterpret_cast<const char*>(guid), 16);
    o.write("data", 4); wr32(o, data_size);
    for (std::size_t i = 0; i < frames; ++i) {
        const auto left = static_cast<std::int16_t>(1000 + static_cast<int>(i));
        const auto right = static_cast<std::int16_t>(-1000 - static_cast<int>(i));
        wr16(o, static_cast<std::uint16_t>(left));
        wr16(o, static_cast<std::uint16_t>(right));
    }
}
}

int main() {
    fv1::ValidationAudio reference;
    std::string error;
    check(fv1::generate_validation_stimulus(reference, 48000, 1.0, "multitone", 0.35, 440.0, 16000.0, 1234, &error),
          "generate multitone stimulus");
    check(reference.frames.size() == 48000, "stimulus duration");

    fv1::ValidationAudio capture;
    capture.sample_rate = 48000;
    constexpr std::size_t delay = 37;
    constexpr float gain = 0.8f;
    capture.frames.assign(reference.frames.size() + delay, {});
    for (std::size_t i = 0; i < reference.frames.size(); ++i) {
        capture.frames[i + delay].left = reference.frames[i].left * gain;
        capture.frames[i + delay].right = reference.frames[i].right * gain;
    }

    fv1::ValidationConfig cfg;
    cfg.max_alignment_ms = 5.0;
    cfg.gain_match_residual = true;
    cfg.minimum_correlation = 0.9999;
    cfg.maximum_residual_rms_dbfs = -100.0;
    cfg.maximum_residual_peak_dbfs = -90.0;
    cfg.fft_size = 4096;
    const auto result = fv1::validate_recordings(reference, capture, cfg);
    check(result.capture_delay_frames == static_cast<std::int64_t>(delay), "recover capture delay");
    check(std::abs(result.left.gain_error_db - 20.0 * std::log10(0.8)) < 0.02, "report raw gain error");
    check(result.left.correlation > 0.999999, "left correlation");
    check(result.right.correlation > 0.999999, "right correlation");
    check(result.left.residual_rms_dbfs < -120.0, "gain-matched residual left");
    check(result.passed, "known delay/gain capture passes configured limits");
    check(!result.frequency_response.empty(), "frequency response produced");

    fv1::ValidationAudio pure_sine;
    check(fv1::generate_validation_stimulus(pure_sine, 48000, 0.1, "sine", 0.2, 1000.0, 1000.0, 1, &error),
          "generate pure sine stimulus");
    const auto identical_sine = fv1::validate_recordings(pure_sine, pure_sine);
    check(identical_sine.capture_delay_frames == 0, "periodic identical signal prefers zero-lag alignment");
    check(identical_sine.passed, "identical sine passes default limits");

    const fs::path dir = fs::temp_directory_path() / "fv1-validation-test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path wav = dir / "reference.wav";
    check(fv1::write_validation_wav(wav, reference, &error), "write validation WAV");
    fv1::ValidationAudio reread;
    check(fv1::load_validation_wav(wav, reread, &error), "read validation WAV");
    check(reread.sample_rate == reference.sample_rate && reread.frames.size() == reference.frames.size(), "WAV roundtrip shape");

    const fs::path extensible = dir / "extensible-pcm16.wav";
    make_extensible_pcm16_wav(extensible, 48000);
    fv1::ValidationAudio extensible_audio;
    check(fv1::load_validation_wav(extensible, extensible_audio, &error), "validation WAVE_FORMAT_EXTENSIBLE PCM16 load");
    check(extensible_audio.sample_rate == 48000 && extensible_audio.frames.size() == 128,
          "validation extensible WAV shape");
    check(extensible_audio.frames.front().left > 0.0f && extensible_audio.frames.front().right < 0.0f,
          "validation extensible WAV channel decode");

    check(fv1::write_validation_report_bundle(dir / "known-capture", result, &error), "write report bundle");
    check(fs::exists(dir / "known-capture.json"), "JSON report exists");
    check(fs::exists(dir / "known-capture.md"), "Markdown report exists");
    check(fs::exists(dir / "known-capture-frequency.csv"), "frequency CSV exists");
    check(fs::exists(dir / "known-capture-residual.wav"), "residual WAV exists");

    auto broken = capture;
    for (std::size_t i = delay; i < broken.frames.size(); i += 11) broken.frames[i].left += 0.2f;
    cfg.gain_match_residual = false;
    cfg.minimum_correlation = 0.99999;
    cfg.maximum_residual_rms_dbfs = -60.0;
    const auto fail_result = fv1::validate_recordings(reference, broken, cfg);
    check(!fail_result.passed && !fail_result.failures.empty(), "bad capture fails limits");

    if (failures) return 1;
    std::cout << "fv1 validation tests passed\n";
    return 0;
}
