#include <fv1/validation.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <string>

namespace fs = std::filesystem;

namespace {

fs::path unicode_component() {
    return fs::path(
        std::u8string(u8"FV1-Ünicode-測試"));
}

bool contains_partial_file(const fs::path& root) {
    std::error_code ec;
    if (!fs::exists(root, ec))
        return false;

    for (fs::recursive_directory_iterator it(
             root,
             fs::directory_options::skip_permission_denied,
             ec),
         end;
         !ec && it != end;
         it.increment(ec)) {
        if (!it->is_regular_file(ec))
            continue;
        const auto name = it->path().filename().u8string();
        if (name.find(std::u8string(u8".partial-"))
            != std::u8string::npos) {
            return true;
        }
    }
    return false;
}

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

    fv1::ValidationPackConfig pack_cfg;
    pack_cfg.sample_rate = 16000;
    pack_cfg.standard_seconds = 0.05;
    pack_cfg.level = 0.2;
    pack_cfg.seed = 123u;
    const fs::path pack = dir / "hardware-pack";
    check(fv1::write_validation_stimulus_pack(pack, pack_cfg, &error), "write hardware validation stimulus pack");
    check(fs::exists(pack / "01-impulse.wav"), "validation pack impulse exists");
    check(fs::exists(pack / "02-multitone.wav"), "validation pack multitone exists");
    check(fs::exists(pack / "03-log-sweep.wav"), "validation pack sweep exists");
    check(fs::exists(pack / "04-sine-1khz.wav"), "validation pack sine exists");
    check(fs::exists(pack / "05-white-noise.wav"), "validation pack white noise exists");
    check(fs::exists(pack / "06-pink-noise.wav"), "validation pack pink noise exists");
    check(fs::exists(pack / "manifest.json") && fs::exists(pack / "README.txt"), "validation pack manifest/readme exist");

    const fs::path unicode_dir =
        dir / unicode_component() / "validation export";
    fs::create_directories(unicode_dir);

    const fs::path unicode_wav =
        unicode_dir
        / fs::path(std::u8string(u8"référence-測試.wav"));
    check(
        fv1::write_validation_wav(
            unicode_wav,
            reference,
            &error),
        "write Unicode validation WAV");

    fv1::ValidationAudio unicode_reread;
    check(
        fv1::load_validation_wav(
            unicode_wav,
            unicode_reread,
            &error),
        "read Unicode validation WAV");
    check(
        unicode_reread.frames.size() == reference.frames.size(),
        "Unicode validation WAV shape");

    const fs::path unicode_prefix =
        unicode_dir
        / fs::path(std::u8string(u8"rapport-Áudio-測試"));
    check(
        fv1::write_validation_report_bundle(
            unicode_prefix,
            result,
            &error),
        "write Unicode validation report bundle");

    fs::path unicode_json = unicode_prefix;
    unicode_json += ".json";
    fs::path unicode_md = unicode_prefix;
    unicode_md += ".md";
    fs::path unicode_csv = unicode_prefix;
    unicode_csv += "-frequency.csv";
    fs::path unicode_residual = unicode_prefix;
    unicode_residual += "-residual.wav";

    check(fs::exists(unicode_json), "Unicode JSON report exists");
    check(fs::exists(unicode_md), "Unicode Markdown report exists");
    check(fs::exists(unicode_csv), "Unicode frequency CSV exists");
    check(fs::exists(unicode_residual), "Unicode residual WAV exists");

    const fs::path unicode_pack =
        unicode_dir
        / fs::path(std::u8string(u8"hardware-pack-測試"));
    check(
        fv1::write_validation_stimulus_pack(
            unicode_pack,
            pack_cfg,
            &error),
        "write Unicode validation pack");
    check(
        fs::exists(unicode_pack / "manifest.json")
            && fs::exists(unicode_pack / "README.txt"),
        "Unicode validation pack metadata exists");

    check(
        !contains_partial_file(unicode_dir),
        "validation exports leave no .partial artifacts");

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
