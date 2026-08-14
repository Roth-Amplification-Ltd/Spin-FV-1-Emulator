#include <fv1/audio_source.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>

namespace fv1 {
namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;

std::uint16_t rd16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}
std::uint32_t rd32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

struct DecodedWav {
    std::uint32_t sample_rate{};
    std::vector<StereoFrame> frames;
};

bool read_wav(const std::filesystem::path& path, DecodedWav& out, std::string* error) {
    auto fail = [&](const std::string& text) {
        if (error) *error = text;
        return false;
    };

    std::ifstream f(path, std::ios::binary);
    if (!f) return fail("cannot open WAV: " + path.string());
    f.seekg(0, std::ios::end);
    const std::streamoff n = f.tellg();
    f.seekg(0, std::ios::beg);
    if (n < 44) return fail("WAV is too small");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(n));
    f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!f) return fail("could not read WAV");
    if (std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
        return fail("not a RIFF/WAVE file");

    std::uint16_t format = 0, channels = 0, bits = 0, valid_bits = 0;
    std::uint32_t sample_rate = 0;
    const std::uint8_t* audio = nullptr;
    std::uint32_t audio_size = 0;
    std::size_t p = 12;
    while (p + 8 <= bytes.size()) {
        const std::uint8_t* h = bytes.data() + p;
        const std::uint32_t sz = rd32(h + 4);
        const std::size_t data_pos = p + 8;
        if (data_pos + sz > bytes.size()) return fail("truncated WAV chunk");
        if (std::memcmp(h, "fmt ", 4) == 0) {
            if (sz < 16) return fail("short WAV fmt chunk");
            format = rd16(bytes.data() + data_pos);
            channels = rd16(bytes.data() + data_pos + 2);
            sample_rate = rd32(bytes.data() + data_pos + 4);
            bits = rd16(bytes.data() + data_pos + 14);
            valid_bits = bits;

            /* WAVE_FORMAT_EXTENSIBLE is common in DAW/exported WAV files.
               Accept the standard PCM and IEEE-float subformats when the
               container and valid sample widths match the formats decoded
               below.  Packed-valid-bits-in-a-larger-container variants stay
               explicitly unsupported rather than being scaled incorrectly. */
            if (format == 0xfffe) {
                if (sz < 40) return fail("short WAVE_FORMAT_EXTENSIBLE fmt chunk");
                valid_bits = rd16(bytes.data() + data_pos + 18);
                const std::uint8_t* guid = bytes.data() + data_pos + 24;
                static constexpr std::uint8_t guid_tail[12] = {
                    0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71
                };
                if (std::memcmp(guid + 4, guid_tail, sizeof(guid_tail)) != 0)
                    return fail("unsupported WAVE_FORMAT_EXTENSIBLE subformat GUID");
                const std::uint32_t subformat = rd32(guid);
                if (subformat == 1u || subformat == 3u)
                    format = static_cast<std::uint16_t>(subformat);
                else
                    return fail("unsupported WAVE_FORMAT_EXTENSIBLE audio subtype");
            }
        } else if (std::memcmp(h, "data", 4) == 0) {
            audio = bytes.data() + data_pos;
            audio_size = sz;
        }
        p = data_pos + sz + (sz & 1u);
    }

    if (!audio || sample_rate == 0 || (channels != 1 && channels != 2))
        return fail("unsupported or incomplete WAV");
    if (valid_bits == 0) valid_bits = bits;
    if (valid_bits != bits)
        return fail("WAV valid-bits/container-width mismatch is not supported yet");
    const bool pcm_ok = format == 1 && (bits == 16 || bits == 24 || bits == 32);
    const bool float_ok = format == 3 && bits == 32;
    if (!pcm_ok && !float_ok)
        return fail("WAV supports PCM16/24/32 or IEEE float32 only");

    const std::size_t bytes_per_sample = bits / 8u;
    const std::size_t frame_bytes = bytes_per_sample * channels;
    if (frame_bytes == 0) return fail("invalid WAV frame size");
    const std::size_t frame_count = audio_size / frame_bytes;

    auto decode = [&](const std::uint8_t* s) -> float {
        if (format == 3) {
            float v{};
            std::memcpy(&v, s, sizeof(v));
            return std::clamp(v, -1.0f, 1.0f);
        }
        if (bits == 16) {
            const std::int16_t v = static_cast<std::int16_t>(rd16(s));
            return static_cast<float>(static_cast<double>(v) / 32768.0);
        }
        if (bits == 24) {
            std::int32_t v = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(s[0]) |
                (static_cast<std::uint32_t>(s[1]) << 8) |
                (static_cast<std::uint32_t>(s[2]) << 16));
            if ((v & 0x00800000) != 0) v |= ~0x00ffffff;
            return static_cast<float>(static_cast<double>(v) / 8388608.0);
        }
        const std::int32_t v = static_cast<std::int32_t>(rd32(s));
        return static_cast<float>(static_cast<double>(v) / 2147483648.0);
    };

    out.sample_rate = sample_rate;
    out.frames.resize(frame_count);
    for (std::size_t i = 0; i < frame_count; ++i) {
        const auto* frame = audio + i * frame_bytes;
        const float l = decode(frame);
        const float r = channels == 2 ? decode(frame + bytes_per_sample) : l;
        out.frames[i] = {l, r};
    }
    return true;
}

float fast_random_bipolar(std::uint32_t& state) noexcept {
    // xorshift32: deterministic and adequate for a lab stimulus generator.
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    const double unit = static_cast<double>(state) /
                        static_cast<double>(std::numeric_limits<std::uint32_t>::max());
    return static_cast<float>(unit * 2.0 - 1.0);
}

} // namespace

bool LiveInputSource::prepare(double, std::size_t) { return true; }
void LiveInputSource::reset() noexcept {}
void LiveInputSource::render(const StereoFrame* live_input, StereoFrame* destination,
                             std::size_t frames) noexcept {
    if (!destination) return;
    if (!live_input) {
        std::fill_n(destination, frames, StereoFrame{});
        return;
    }
    std::memcpy(destination, live_input, frames * sizeof(StereoFrame));
}

class FileLoopSource::Impl {
public:
    DecodedWav wav;
    double host_rate{48000.0};
    double position{};
    std::uint64_t loop_begin{};
    std::uint64_t loop_end{};
    bool do_loop{true};
    TransportState transport{TransportState::Stopped};
};

FileLoopSource::FileLoopSource() : impl_(std::make_unique<Impl>()) {}
FileLoopSource::~FileLoopSource() = default;

bool FileLoopSource::load(const std::filesystem::path& path, std::string* error) {
    DecodedWav decoded;
    if (!read_wav(path, decoded, error)) return false;
    if (decoded.frames.empty()) {
        if (error) *error = "WAV contains no audio frames";
        return false;
    }
    impl_->wav = std::move(decoded);
    impl_->loop_begin = 0;
    impl_->loop_end = impl_->wav.frames.size();
    impl_->position = 0.0;
    impl_->transport = TransportState::Stopped;
    return true;
}

bool FileLoopSource::prepare(double host_sample_rate, std::size_t) {
    if (!(host_sample_rate > 1000.0) || impl_->wav.frames.empty() || impl_->wav.sample_rate == 0)
        return false;
    impl_->host_rate = host_sample_rate;
    return true;
}

void FileLoopSource::reset() noexcept {
    impl_->position = static_cast<double>(impl_->loop_begin);
}
void FileLoopSource::play() noexcept { if (!impl_->wav.frames.empty()) impl_->transport = TransportState::Playing; }
void FileLoopSource::pause() noexcept { if (impl_->transport == TransportState::Playing) impl_->transport = TransportState::Paused; }
void FileLoopSource::stop() noexcept { impl_->transport = TransportState::Stopped; reset(); }
void FileLoopSource::set_looping(bool enabled) noexcept { impl_->do_loop = enabled; }
bool FileLoopSource::looping() const noexcept { return impl_->do_loop; }
TransportState FileLoopSource::state() const noexcept { return impl_->transport; }

bool FileLoopSource::set_loop_region(std::uint64_t begin, std::uint64_t end) noexcept {
    const std::uint64_t total = impl_->wav.frames.size();
    if (end == 0) end = total;
    if (begin >= end || end > total) return false;
    impl_->loop_begin = begin;
    impl_->loop_end = end;
    if (impl_->position < static_cast<double>(begin) || impl_->position >= static_cast<double>(end))
        impl_->position = static_cast<double>(begin);
    return true;
}

std::uint64_t FileLoopSource::total_frames() const noexcept { return impl_->wav.frames.size(); }
std::uint32_t FileLoopSource::file_sample_rate() const noexcept { return impl_->wav.sample_rate; }
double FileLoopSource::position_seconds() const noexcept {
    return impl_->wav.sample_rate == 0 ? 0.0 : impl_->position / static_cast<double>(impl_->wav.sample_rate);
}

void FileLoopSource::render(const StereoFrame*, StereoFrame* destination,
                            std::size_t frames) noexcept {
    if (!destination) return;
    if (impl_->transport != TransportState::Playing || impl_->wav.frames.empty()) {
        std::fill_n(destination, frames, StereoFrame{});
        return;
    }

    const double step = static_cast<double>(impl_->wav.sample_rate) / impl_->host_rate;
    const double begin = static_cast<double>(impl_->loop_begin);
    const double end = static_cast<double>(impl_->loop_end);
    const double span = end - begin;

    for (std::size_t out = 0; out < frames; ++out) {
        if (impl_->position >= end) {
            if (!impl_->do_loop) {
                impl_->transport = TransportState::Stopped;
                std::fill(destination + static_cast<std::ptrdiff_t>(out),
                          destination + static_cast<std::ptrdiff_t>(frames), StereoFrame{});
                return;
            }
            impl_->position = begin + std::fmod(impl_->position - begin, span);
        }

        const auto i0 = static_cast<std::uint64_t>(std::floor(impl_->position));
        const double frac = impl_->position - static_cast<double>(i0);
        std::uint64_t i1 = i0 + 1;
        if (i1 >= impl_->loop_end) i1 = impl_->do_loop ? impl_->loop_begin : i0;

        const StereoFrame a = impl_->wav.frames[static_cast<std::size_t>(i0)];
        const StereoFrame b = impl_->wav.frames[static_cast<std::size_t>(i1)];
        destination[out] = {
            static_cast<float>(a.left + (b.left - a.left) * frac),
            static_cast<float>(a.right + (b.right - a.right) * frac)
        };
        impl_->position += step;
    }
}

TestSignalSource::TestSignalSource(TestSignalConfig config) : config_(config), rng_(config.noise_seed) {}
bool TestSignalSource::prepare(double host_sample_rate, std::size_t) {
    if (!(host_sample_rate > 1000.0)) return false;
    sample_rate_ = host_sample_rate;
    reset();
    return true;
}
void TestSignalSource::reset() noexcept {
    phase_ = 0.0;
    sample_index_ = 0;
    rng_ = config_.noise_seed ? config_.noise_seed : 1u;
    pink0_ = pink1_ = pink2_ = 0.0;
}
void TestSignalSource::configure(const TestSignalConfig& config) noexcept { config_ = config; reset(); }
TestSignalConfig TestSignalSource::config() const noexcept { return config_; }

void TestSignalSource::render(const StereoFrame*, StereoFrame* destination,
                              std::size_t frames) noexcept {
    if (!destination) return;
    const double amp = std::clamp(config_.amplitude, 0.0, 1.0);
    for (std::size_t i = 0; i < frames; ++i, ++sample_index_) {
        double v = 0.0;
        switch (config_.kind) {
        case TestSignalKind::Sine: {
            v = std::sin(phase_) * amp;
            phase_ += 2.0 * kPi * std::max(0.0, config_.frequency_hz) / sample_rate_;
            if (phase_ >= 2.0 * kPi) phase_ = std::fmod(phase_, 2.0 * kPi);
            break;
        }
        case TestSignalKind::Sweep: {
            const double duration = std::max(0.001, config_.sweep_seconds);
            const double t = std::fmod(static_cast<double>(sample_index_) / sample_rate_, duration) / duration;
            const double f0 = std::max(1.0, config_.frequency_hz);
            const double f1 = std::max(f0, config_.sweep_end_hz);
            const double frequency = f0 * std::pow(f1 / f0, t); // log sweep
            v = std::sin(phase_) * amp;
            phase_ += 2.0 * kPi * frequency / sample_rate_;
            if (phase_ >= 2.0 * kPi) phase_ = std::fmod(phase_, 2.0 * kPi);
            break;
        }
        case TestSignalKind::WhiteNoise:
            v = static_cast<double>(fast_random_bipolar(rng_)) * amp;
            break;
        case TestSignalKind::PinkNoise: {
            const double white = fast_random_bipolar(rng_);
            // Paul Kellet-style three-pole approximation; deterministic, cheap,
            // and sufficient for a lab stimulus rather than mastering noise.
            pink0_ = 0.99765 * pink0_ + white * 0.0990460;
            pink1_ = 0.96300 * pink1_ + white * 0.2965164;
            pink2_ = 0.57000 * pink2_ + white * 1.0526913;
            v = std::clamp((pink0_ + pink1_ + pink2_ + white * 0.1848) * 0.05, -1.0, 1.0) * amp;
            break;
        }
        case TestSignalKind::Impulse: {
            const std::uint64_t period = std::max<std::uint64_t>(
                1, static_cast<std::uint64_t>(std::llround(std::max(0.001, config_.impulse_period_seconds) * sample_rate_)));
            v = (sample_index_ % period == 0) ? amp : 0.0;
            break;
        }
        }
        const float s = static_cast<float>(v);
        destination[i] = {s, s};
    }
}

} // namespace fv1
