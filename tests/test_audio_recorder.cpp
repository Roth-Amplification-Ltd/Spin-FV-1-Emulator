#include <fv1/audio_recorder.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::filesystem::path unicode_component() {
    return std::filesystem::path(
        std::u8string(u8"FV1-Ünicode-測試"));
}

bool contains_partial_file(
    const std::filesystem::path& root
) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec))
        return false;

    for (std::filesystem::recursive_directory_iterator it(
             root,
             std::filesystem::directory_options::skip_permission_denied,
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

std::uint32_t read_u32(std::ifstream& in) {
    std::array<unsigned char, 4> b{};
    in.read(reinterpret_cast<char*>(b.data()), 4);
    return static_cast<std::uint32_t>(b[0]) |
           (static_cast<std::uint32_t>(b[1]) << 8u) |
           (static_cast<std::uint32_t>(b[2]) << 16u) |
           (static_cast<std::uint32_t>(b[3]) << 24u);
}

bool valid_float_wav(const std::filesystem::path& path, std::uint32_t expected_rate,
                     std::uint32_t expected_data_bytes) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::array<char, 4> id{};
    in.read(id.data(), 4);
    if (std::string(id.data(), 4) != "RIFF") return false;
    (void)read_u32(in);
    in.read(id.data(), 4);
    if (std::string(id.data(), 4) != "WAVE") return false;
    in.read(id.data(), 4);
    if (std::string(id.data(), 4) != "fmt ") return false;
    if (read_u32(in) != 16u) return false;
    std::array<unsigned char, 4> fmt{};
    in.read(reinterpret_cast<char*>(fmt.data()), 4);
    const std::uint16_t format = static_cast<std::uint16_t>(fmt[0] | (fmt[1] << 8u));
    const std::uint16_t channels = static_cast<std::uint16_t>(fmt[2] | (fmt[3] << 8u));
    if (format != 3u || channels != 2u) return false;
    if (read_u32(in) != expected_rate) return false;
    (void)read_u32(in); // byte rate
    std::array<unsigned char, 4> align_bits{};
    in.read(reinterpret_cast<char*>(align_bits.data()), 4);
    in.read(id.data(), 4);
    if (std::string(id.data(), 4) != "data") return false;
    return read_u32(in) == expected_data_bytes;
}

} // namespace

int main() {
    constexpr std::uint32_t rate = 48000;
    constexpr std::size_t frames = 4096;
    const auto temp = std::filesystem::temp_directory_path() / "fv1-recorder-test.wav";
    const auto raw = temp.parent_path() / "fv1-recorder-test-raw.wav";
    const auto processed = temp.parent_path() / "fv1-recorder-test-processed.wav";
    std::error_code ec;
    std::filesystem::remove(raw, ec);
    std::filesystem::remove(processed, ec);

    fv1::AudioRecorder recorder;
    std::string error;
    if (!recorder.prepare(temp, rate, fv1::AudioRecordMode::RawAndProcessed, 16384, &error)) {
        std::cerr << error << '\n';
        return 1;
    }
    if (!recorder.start(&error)) {
        std::cerr << error << '\n';
        return 1;
    }

    std::vector<fv1::StereoFrame> block(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const float x = 0.25f * std::sin(static_cast<float>(2.0 * 3.14159265358979323846 * 440.0 *
                                                             static_cast<double>(i) / rate));
        block[i] = {x, -x};
    }
    recorder.push_raw(block.data(), block.size());
    recorder.push_processed(block.data(), block.size());
    recorder.stop();

    const auto stats = recorder.stats();
    if (stats.raw_frames_dropped != 0 || stats.processed_frames_dropped != 0 ||
        stats.raw_frames_written != frames || stats.processed_frames_written != frames) {
        std::cerr << "unexpected recorder stats\n";
        return 1;
    }

    constexpr std::uint32_t bytes = static_cast<std::uint32_t>(frames * sizeof(float) * 2u);
    if (!valid_float_wav(raw, rate, bytes) || !valid_float_wav(processed, rate, bytes)) {
        std::cerr << "invalid recorder WAV output\n";
        return 1;
    }

    std::filesystem::remove(raw, ec);
    std::filesystem::remove(processed, ec);

    const auto unicode_dir =
        std::filesystem::temp_directory_path()
        / unicode_component()
        / "nested recording folder";
    std::filesystem::remove_all(unicode_dir, ec);
    std::filesystem::create_directories(unicode_dir, ec);
    if (ec) {
        std::cerr << "could not create Unicode recorder test directory\n";
        return 1;
    }

    const auto unicode_base =
        unicode_dir
        / std::filesystem::path(
            std::u8string(u8"capture-Áudio-測試.wav"));
    const auto unicode_raw =
        unicode_dir
        / std::filesystem::path(
            std::u8string(u8"capture-Áudio-測試-raw.wav"));
    const auto unicode_processed =
        unicode_dir
        / std::filesystem::path(
            std::u8string(u8"capture-Áudio-測試-processed.wav"));

    if (!recorder.prepare(
            unicode_base,
            rate,
            fv1::AudioRecordMode::RawAndProcessed,
            16384,
            &error)
        || !recorder.start(&error)) {
        std::cerr
            << "Unicode recorder prepare/start failed: "
            << error
            << '\n';
        return 1;
    }

    recorder.push_raw(block.data(), block.size());
    recorder.push_processed(block.data(), block.size());
    recorder.stop();

    if (!recorder.last_error().empty()) {
        std::cerr
            << "Unicode recorder finalization failed: "
            << recorder.last_error()
            << '\n';
        return 1;
    }

    if (!valid_float_wav(unicode_raw, rate, bytes)
        || !valid_float_wav(unicode_processed, rate, bytes)) {
        std::cerr << "Unicode recorder WAV output invalid\n";
        return 1;
    }

    if (contains_partial_file(unicode_dir)) {
        std::cerr
            << "transactional recorder left a .partial file\n";
        return 1;
    }

    std::filesystem::remove_all(unicode_dir, ec);

    std::cout << "fv1 recorder tests passed\n";
    return 0;
}
