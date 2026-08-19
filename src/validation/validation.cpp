#include <fv1/validation.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fv1 {
namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr double kTiny = 1.0e-20;

bool fail(std::string* error, const std::string& message) {
    if (error) *error = message;
    return false;
}

std::string path_utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return std::string(
        reinterpret_cast<const char*>(bytes.data()),
        bytes.size());
}

std::filesystem::path append_ascii_suffix(
    const std::filesystem::path& base,
    const char* suffix
) {
    std::filesystem::path out = base;
    out += std::filesystem::path(suffix);
    return out;
}

std::filesystem::path make_temp_sibling(
    const std::filesystem::path& final_path
) {
    static std::atomic<std::uint64_t> sequence{0};
    const auto id =
        sequence.fetch_add(1, std::memory_order_relaxed);
    const auto ticks = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count());

    std::filesystem::path temp = final_path;
    temp += std::filesystem::path(".partial-");
    temp += std::filesystem::path(
        std::to_string(ticks)
        + "-"
        + std::to_string(id));
    return temp;
}

bool replace_completed_file(
    const std::filesystem::path& temp,
    const std::filesystem::path& final_path,
    std::string* error
) {
#if defined(_WIN32)
    if (::MoveFileExW(
            temp.c_str(),
            final_path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) {
        return true;
    }

    return fail(
        error,
        "cannot finalize "
            + path_utf8(final_path)
            + " (Windows error "
            + std::to_string(::GetLastError())
            + ")");
#else
    std::error_code ec;
    std::filesystem::rename(temp, final_path, ec);
    if (!ec) return true;
    return fail(
        error,
        "cannot finalize "
            + path_utf8(final_path)
            + ": "
            + ec.message());
#endif
}

void remove_quietly(
    const std::filesystem::path& path
) noexcept {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

class FileTransaction {
public:
    ~FileTransaction() {
        if (committed_) return;
        for (const auto& entry : files_)
            remove_quietly(entry.first);
    }

    std::filesystem::path stage(
        const std::filesystem::path& final_path
    ) {
        auto temp = make_temp_sibling(final_path);
        remove_quietly(temp);
        files_.push_back({temp, final_path});
        return temp;
    }

    bool commit(std::string* error) {
        for (const auto& entry : files_) {
            if (!replace_completed_file(
                    entry.first,
                    entry.second,
                    error)) {
                return false;
            }
        }
        committed_ = true;
        return true;
    }

private:
    std::vector<std::pair<
        std::filesystem::path,
        std::filesystem::path>> files_;
    bool committed_{};
};

std::uint16_t rd16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}
std::uint32_t rd32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}
void wr16(std::ostream& o, std::uint16_t v) {
    const char b[2]{static_cast<char>(v), static_cast<char>(v >> 8)};
    o.write(b, 2);
}
void wr32(std::ostream& o, std::uint32_t v) {
    const char b[4]{static_cast<char>(v), static_cast<char>(v >> 8),
                    static_cast<char>(v >> 16), static_cast<char>(v >> 24)};
    o.write(b, 4);
}

double dbfs_from_rms(double value) {
    return 20.0 * std::log10(std::max(value, 1.0e-10));
}

double db_from_ratio(double value) {
    return 20.0 * std::log10(std::max(std::abs(value), 1.0e-10));
}

double mono(const StereoFrame& f) noexcept {
    return 0.5 * (static_cast<double>(f.left) + static_cast<double>(f.right));
}

bool is_power_of_two(std::size_t n) noexcept {
    return n >= 2 && (n & (n - 1)) == 0;
}

std::size_t floor_power_of_two(std::size_t n) noexcept {
    if (n < 2) return 0;
    std::size_t p = 1;
    while (p <= n / 2) p <<= 1;
    return p;
}

void fft_in_place(std::vector<std::complex<double>>& a) {
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double angle = -2.0 * kPi / static_cast<double>(len);
        const std::complex<double> wlen(std::cos(angle), std::sin(angle));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (std::size_t j = 0; j < len / 2; ++j) {
                const auto u = a[i + j];
                const auto v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

std::int64_t estimate_delay(const std::vector<StereoFrame>& reference,
                            const std::vector<StereoFrame>& capture,
                            std::size_t max_lag) {
    if (reference.empty() || capture.empty()) return 0;
    const std::size_t common = std::min(reference.size(), capture.size());
    // Bound the correlation workload for long captures while preserving a
    // useful wide slice. A deterministic stride keeps test runs reproducible.
    const std::size_t target = std::min<std::size_t>(common, 32768);
    const std::size_t stride = std::max<std::size_t>(1, common / std::max<std::size_t>(1, target));

    double best = -2.0;
    std::int64_t best_lag = 0;
    const auto max_signed = static_cast<std::int64_t>(max_lag);
    for (std::int64_t lag = -max_signed; lag <= max_signed; ++lag) {
        double sum_xy = 0.0, sum_x2 = 0.0, sum_y2 = 0.0;
        std::size_t samples = 0;
        const std::int64_t ref_begin = std::max<std::int64_t>(0, -lag);
        const std::int64_t cap_begin = std::max<std::int64_t>(0, lag);
        const std::size_t available = std::min(
            reference.size() - static_cast<std::size_t>(ref_begin),
            capture.size() - static_cast<std::size_t>(cap_begin));
        for (std::size_t off = 0; off < available; off += stride) {
            const double x = mono(reference[static_cast<std::size_t>(ref_begin) + off]);
            const double y = mono(capture[static_cast<std::size_t>(cap_begin) + off]);
            sum_xy += x * y;
            sum_x2 += x * x;
            sum_y2 += y * y;
            ++samples;
        }
        if (samples < 32) continue;
        const double denom = std::sqrt(sum_x2 * sum_y2);
        const double corr = denom > kTiny ? sum_xy / denom : -1.0;
        constexpr double tie_epsilon = 1.0e-10;
        if (corr > best + tie_epsilon ||
            (std::abs(corr - best) <= tie_epsilon && std::abs(lag) < std::abs(best_lag))) {
            best = corr;
            best_lag = lag;
        }
    }
    return best_lag;
}

struct AlignedViews {
    std::size_t ref_begin{};
    std::size_t cap_begin{};
    std::size_t frames{};
};

AlignedViews aligned_views(std::size_t ref_size, std::size_t cap_size, std::int64_t lag) {
    AlignedViews v;
    if (lag >= 0) v.cap_begin = static_cast<std::size_t>(lag);
    else v.ref_begin = static_cast<std::size_t>(-lag);
    if (v.ref_begin >= ref_size || v.cap_begin >= cap_size) return v;
    v.frames = std::min(ref_size - v.ref_begin, cap_size - v.cap_begin);
    return v;
}

ValidationChannelMetrics channel_metrics(const ValidationAudio& ref,
                                         const ValidationAudio& cap,
                                         const AlignedViews& v,
                                         bool left,
                                         double residual_gain,
                                         std::vector<StereoFrame>* residual) {
    ValidationChannelMetrics m;
    if (v.frames == 0) return m;
    double ref2 = 0.0, cap2 = 0.0, cross = 0.0, res2 = 0.0, peak = 0.0;
    for (std::size_t i = 0; i < v.frames; ++i) {
        const StereoFrame& rf = ref.frames[v.ref_begin + i];
        const StereoFrame& cf = cap.frames[v.cap_begin + i];
        const double r = left ? rf.left : rf.right;
        const double c = left ? cf.left : cf.right;
        const double e = c * residual_gain - r;
        ref2 += r * r;
        cap2 += c * c;
        cross += r * c;
        res2 += e * e;
        peak = std::max(peak, std::abs(e));
        if (residual) {
            if (left) (*residual)[i].left = static_cast<float>(e);
            else (*residual)[i].right = static_cast<float>(e);
        }
    }
    const double n = static_cast<double>(v.frames);
    const double rrms = std::sqrt(ref2 / n);
    const double crms = std::sqrt(cap2 / n);
    const double erms = std::sqrt(res2 / n);
    m.reference_rms_dbfs = dbfs_from_rms(rrms);
    m.capture_rms_dbfs = dbfs_from_rms(crms);
    m.gain_error_db = db_from_ratio(crms / std::max(rrms, 1.0e-10));
    m.correlation = (ref2 > kTiny && cap2 > kTiny)
        ? std::clamp(cross / std::sqrt(ref2 * cap2), -1.0, 1.0) : 0.0;
    m.residual_rms_dbfs = dbfs_from_rms(erms);
    m.residual_peak_dbfs = dbfs_from_rms(peak);
    m.snr_db = 20.0 * std::log10(std::max(rrms, 1.0e-10) / std::max(erms, 1.0e-10));
    return m;
}

double least_squares_gain(const ValidationAudio& ref,
                          const ValidationAudio& cap,
                          const AlignedViews& v) {
    double xy = 0.0, yy = 0.0;
    for (std::size_t i = 0; i < v.frames; ++i) {
        const auto& r = ref.frames[v.ref_begin + i];
        const auto& c = cap.frames[v.cap_begin + i];
        xy += static_cast<double>(r.left) * c.left + static_cast<double>(r.right) * c.right;
        yy += static_cast<double>(c.left) * c.left + static_cast<double>(c.right) * c.right;
    }
    return yy > kTiny ? xy / yy : 1.0;
}

std::vector<ValidationFrequencyPoint> spectral_compare(const ValidationAudio& ref,
                                                       const ValidationAudio& cap,
                                                       const AlignedViews& v,
                                                       const ValidationConfig& cfg,
                                                       double gain) {
    if (v.frames < 256 || ref.sample_rate == 0) return {};
    std::size_t n = cfg.fft_size;
    if (!is_power_of_two(n) || n < 256) n = 16384;
    n = std::min(n, floor_power_of_two(v.frames));
    if (n < 256) return {};

    // Choose the highest-energy window in the reference. This makes the
    // spectral view useful for impulses, sweeps and music without guessing a
    // fixed offset into recordings that may contain leading silence.
    std::size_t best_start = 0;
    double best_energy = -1.0;
    const std::size_t hop = std::max<std::size_t>(1, n / 4);
    for (std::size_t start = 0; start + n <= v.frames; start += hop) {
        double e = 0.0;
        for (std::size_t i = 0; i < n; i += 8) {
            const double x = mono(ref.frames[v.ref_begin + start + i]);
            e += x * x;
        }
        if (e > best_energy) { best_energy = e; best_start = start; }
    }

    std::vector<std::complex<double>> a(n), b(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(i) /
                                             static_cast<double>(n - 1));
        a[i] = {mono(ref.frames[v.ref_begin + best_start + i]) * w, 0.0};
        b[i] = {mono(cap.frames[v.cap_begin + best_start + i]) * gain * w, 0.0};
    }
    fft_in_place(a);
    fft_in_place(b);

    const std::size_t bins = n / 2 + 1;
    std::vector<ValidationFrequencyPoint> out;
    out.reserve(bins / 4);
    const double fft_scale = 2.0 / static_cast<double>(n);
    // Decimate the report to roughly <= 1024 points. GUI/report readers do not
    // need tens of thousands of adjacent bins to see response errors.
    const std::size_t bin_step = std::max<std::size_t>(1, bins / 1024);
    for (std::size_t k = 1; k < bins; k += bin_step) {
        const double ref_mag = std::abs(a[k]) * fft_scale;
        const double ref_db = dbfs_from_rms(ref_mag);
        if (ref_db < cfg.spectral_floor_db) continue;
        const double cap_mag = std::abs(b[k]) * fft_scale;
        const std::complex<double> ratio = b[k] / (std::abs(a[k]) > 1.0e-15 ? a[k] : std::complex<double>(1.0e-15, 0.0));
        double phase = std::arg(ratio) * 180.0 / kPi;
        while (phase > 180.0) phase -= 360.0;
        while (phase < -180.0) phase += 360.0;
        out.push_back({
            static_cast<double>(k) * static_cast<double>(ref.sample_rate) / static_cast<double>(n),
            db_from_ratio(cap_mag / std::max(ref_mag, 1.0e-15)),
            phase,
            ref_db
        });
    }
    return out;
}

std::string json_escape(const std::string& s) {
    std::ostringstream o;
    for (char raw : s) {
        const auto c = static_cast<unsigned char>(raw);
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (c < 0x20) o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
                else o << static_cast<char>(c);
        }
    }
    return o.str();
}

} // namespace

bool load_validation_wav(const std::filesystem::path& path,
                         ValidationAudio& audio,
                         std::string* error) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return fail(
            error,
            "cannot open " + path_utf8(path));
    f.seekg(0, std::ios::end);
    const auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    if (size < 44) return fail(error, "WAV file is too short");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!f || std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data() + 8, "WAVE", 4))
        return fail(error, "not a RIFF/WAVE file");

    std::uint16_t format = 0, channels = 0, bits = 0, valid_bits = 0;
    std::uint32_t sample_rate = 0;
    const std::uint8_t* data = nullptr;
    std::uint32_t data_size = 0;
    for (std::size_t p = 12; p + 8 <= bytes.size();) {
        const std::uint32_t chunk_size = rd32(bytes.data() + p + 4);
        const std::size_t begin = p + 8;
        if (begin + chunk_size > bytes.size()) return fail(error, "truncated WAV chunk");
        if (!std::memcmp(bytes.data() + p, "fmt ", 4)) {
            if (chunk_size < 16) return fail(error, "short WAV fmt chunk");
            format = rd16(bytes.data() + begin);
            channels = rd16(bytes.data() + begin + 2);
            sample_rate = rd32(bytes.data() + begin + 4);
            bits = rd16(bytes.data() + begin + 14);
            valid_bits = bits;
            if (format == 0xfffe) {
                if (chunk_size < 40) return fail(error, "short WAVE_FORMAT_EXTENSIBLE fmt chunk");
                valid_bits = rd16(bytes.data() + begin + 18);
                const std::uint8_t* guid = bytes.data() + begin + 24;
                // KSDATAFORMAT_SUBTYPE_PCM/FLOAT differ only in the first DWORD.
                const std::uint32_t subtype = rd32(guid);
                static constexpr std::array<std::uint8_t, 12> tail{
                    0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71};
                if (!std::equal(tail.begin(), tail.end(), guid + 4))
                    return fail(error, "unsupported WAVE_FORMAT_EXTENSIBLE subformat GUID");
                if (subtype == 1) format = 1;
                else if (subtype == 3) format = 3;
                else return fail(error, "unsupported WAVE_FORMAT_EXTENSIBLE subtype");
            }
        } else if (!std::memcmp(bytes.data() + p, "data", 4)) {
            data = bytes.data() + begin;
            data_size = chunk_size;
        }
        p = begin + chunk_size + (chunk_size & 1u);
    }
    if (!data || sample_rate == 0 || (channels != 1 && channels != 2))
        return fail(error, "unsupported/incomplete WAV file");
    if (!((format == 1 && (bits == 16 || bits == 24 || bits == 32)) || (format == 3 && bits == 32)))
        return fail(error, "validation WAV reader supports PCM16/24/32 or float32");
    if (valid_bits == 0) valid_bits = bits;

    const std::size_t bytes_per_sample = bits / 8u;
    const std::size_t frame_bytes = bytes_per_sample * channels;
    if (frame_bytes == 0) return fail(error, "invalid WAV frame size");
    const std::size_t frame_count = data_size / frame_bytes;
    ValidationAudio out;
    out.sample_rate = sample_rate;
    out.frames.resize(frame_count);

    auto decode = [&](const std::uint8_t* s) -> float {
        if (format == 3) {
            float v{};
            std::memcpy(&v, s, sizeof(v));
            return std::isfinite(v) ? v : 0.0f;
        }
        if (bits == 16) {
            const auto v = static_cast<std::int16_t>(rd16(s));
            return static_cast<float>(static_cast<double>(v) / 32768.0);
        }
        if (bits == 24) {
            std::int32_t v = static_cast<std::int32_t>(s[0]) |
                             (static_cast<std::int32_t>(s[1]) << 8) |
                             (static_cast<std::int32_t>(s[2]) << 16);
            if ((v & 0x800000) != 0) v |= ~0xffffff;
            return static_cast<float>(static_cast<double>(v) / 8388608.0);
        }
        const auto u = rd32(s);
        const auto v = static_cast<std::int32_t>(u);
        return static_cast<float>(static_cast<double>(v) / 2147483648.0);
    };

    for (std::size_t i = 0; i < frame_count; ++i) {
        const std::uint8_t* p = data + i * frame_bytes;
        out.frames[i].left = decode(p);
        out.frames[i].right = channels == 2 ? decode(p + bytes_per_sample) : out.frames[i].left;
    }
    audio = std::move(out);
    return true;
}

bool write_validation_wav(const std::filesystem::path& path,
                          const ValidationAudio& audio,
                          std::string* error) {
    if (audio.sample_rate == 0)
        return fail(error, "sample rate must be nonzero");
    if (audio.frames.size() >
        (std::numeric_limits<std::uint32_t>::max() - 36u) / 8u) {
        return fail(error, "WAV is too large for RIFF32");
    }

    std::error_code ec;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(
            path.parent_path(),
            ec);
    if (ec) {
        return fail(
            error,
            "cannot create WAV directory for "
                + path_utf8(path)
                + ": "
                + ec.message());
    }

    const auto temp = make_temp_sibling(path);
    remove_quietly(temp);

    std::ofstream o(
        temp,
        std::ios::binary | std::ios::trunc);
    if (!o) {
        return fail(
            error,
            "cannot create WAV staging file for "
                + path_utf8(path));
    }

    const auto data_size =
        static_cast<std::uint32_t>(
            audio.frames.size() * 8u);
    o.write("RIFF", 4);
    wr32(o, 36u + data_size);
    o.write("WAVE", 4);
    o.write("fmt ", 4);
    wr32(o, 16);
    wr16(o, 3);
    wr16(o, 2);
    wr32(o, audio.sample_rate);
    wr32(o, audio.sample_rate * 8u);
    wr16(o, 8);
    wr16(o, 32);
    o.write("data", 4);
    wr32(o, data_size);

    for (const auto& frame : audio.frames) {
        const float l =
            std::isfinite(frame.left)
                ? frame.left
                : 0.0f;
        const float r =
            std::isfinite(frame.right)
                ? frame.right
                : 0.0f;
        o.write(
            reinterpret_cast<const char*>(&l),
            sizeof(l));
        o.write(
            reinterpret_cast<const char*>(&r),
            sizeof(r));
    }

    o.flush();
    const bool write_ok = static_cast<bool>(o);
    o.close();

    if (!write_ok) {
        remove_quietly(temp);
        return fail(
            error,
            "failed while writing "
                + path_utf8(path));
    }

    if (!replace_completed_file(temp, path, error)) {
        remove_quietly(temp);
        return false;
    }
    return true;
}

ValidationResult validate_recordings(const ValidationAudio& reference,
                                     const ValidationAudio& capture,
                                     const ValidationConfig& config) {
    ValidationResult out;
    out.config = config;
    out.sample_rate = reference.sample_rate;
    if (reference.sample_rate == 0 || capture.sample_rate == 0) {
        out.failures.push_back("sample rate is zero");
        return out;
    }
    if (reference.sample_rate != capture.sample_rate) {
        out.failures.push_back("sample rates differ; resample the capture to the reference host rate first");
        return out;
    }
    if (reference.frames.empty() || capture.frames.empty()) {
        out.failures.push_back("reference or capture is empty");
        return out;
    }

    const double max_ms = std::max(0.0, config.max_alignment_ms);
    const std::size_t max_lag = static_cast<std::size_t>(std::llround(
        max_ms * 0.001 * static_cast<double>(reference.sample_rate)));
    out.capture_delay_frames = estimate_delay(reference.frames, capture.frames, max_lag);
    out.capture_delay_ms = 1000.0 * static_cast<double>(out.capture_delay_frames) /
                           static_cast<double>(reference.sample_rate);
    const AlignedViews views = aligned_views(reference.frames.size(), capture.frames.size(),
                                             out.capture_delay_frames);
    out.compared_frames = views.frames;
    if (views.frames < 32) {
        out.failures.push_back("too few aligned frames to compare");
        return out;
    }

    const double gain = config.gain_match_residual ? least_squares_gain(reference, capture, views) : 1.0;
    out.applied_capture_gain_db = db_from_ratio(gain);
    out.residual.assign(views.frames, {});
    out.left = channel_metrics(reference, capture, views, true, gain, &out.residual);
    out.right = channel_metrics(reference, capture, views, false, gain, &out.residual);
    out.frequency_response = spectral_compare(reference, capture, views, config, gain);
    if (!out.frequency_response.empty()) {
        double sum_mag2 = 0.0;
        for (const auto& point : out.frequency_response) {
            sum_mag2 += point.magnitude_error_db * point.magnitude_error_db;
            out.spectral_worst_magnitude_error_db = std::max(
                out.spectral_worst_magnitude_error_db, std::abs(point.magnitude_error_db));
            out.spectral_worst_phase_error_degrees = std::max(
                out.spectral_worst_phase_error_degrees, std::abs(point.phase_error_degrees));
        }
        out.spectral_rms_magnitude_error_db = std::sqrt(
            sum_mag2 / static_cast<double>(out.frequency_response.size()));
    }

    const double corr = std::min(out.left.correlation, out.right.correlation);
    const double residual_rms = std::max(out.left.residual_rms_dbfs, out.right.residual_rms_dbfs);
    const double residual_peak = std::max(out.left.residual_peak_dbfs, out.right.residual_peak_dbfs);
    if (corr < config.minimum_correlation) {
        std::ostringstream s; s << "correlation " << corr << " below minimum " << config.minimum_correlation;
        out.failures.push_back(s.str());
    }
    if (residual_rms > config.maximum_residual_rms_dbfs) {
        std::ostringstream s; s << "residual RMS " << residual_rms << " dBFS above maximum "
                                << config.maximum_residual_rms_dbfs << " dBFS";
        out.failures.push_back(s.str());
    }
    if (residual_peak > config.maximum_residual_peak_dbfs) {
        std::ostringstream s; s << "residual peak " << residual_peak << " dBFS above maximum "
                                << config.maximum_residual_peak_dbfs << " dBFS";
        out.failures.push_back(s.str());
    }
    out.passed = out.failures.empty();
    return out;
}

bool write_validation_report_bundle(const std::filesystem::path& prefix,
                                    const ValidationResult& result,
                                    std::string* error) {
    std::error_code ec;
    if (!prefix.parent_path().empty())
        std::filesystem::create_directories(
            prefix.parent_path(),
            ec);
    if (ec)
        return fail(
            error,
            "cannot create report directory: "
                + ec.message());

    const auto json_path = append_ascii_suffix(prefix, ".json");
    const auto md_path = append_ascii_suffix(prefix, ".md");
    const auto csv_path = append_ascii_suffix(prefix, "-frequency.csv");
    const auto wav_path = append_ascii_suffix(prefix, "-residual.wav");

    FileTransaction transaction;
    const auto json_stage = transaction.stage(json_path);
    const auto md_stage = transaction.stage(md_path);
    const auto csv_stage = transaction.stage(csv_path);
    const auto wav_stage = transaction.stage(wav_path);

    {
        std::ofstream o(json_stage);
        if (!o)
            return fail(
                error,
                "cannot create report staging file for "
                    + path_utf8(json_path));
        o << std::fixed << std::setprecision(9);
        o << "{\n"
          << "  \"schema\": \"spin-fv1-validation-1\",\n"
          << "  \"passed\": " << (result.passed ? "true" : "false") << ",\n"
          << "  \"config\": {\n"
          << "    \"max_alignment_ms\": " << result.config.max_alignment_ms << ",\n"
          << "    \"gain_match_residual\": " << (result.config.gain_match_residual ? "true" : "false") << ",\n"
          << "    \"fft_size\": " << result.config.fft_size << ",\n"
          << "    \"spectral_floor_db\": " << result.config.spectral_floor_db << ",\n"
          << "    \"minimum_correlation\": " << result.config.minimum_correlation << ",\n"
          << "    \"maximum_residual_rms_dbfs\": " << result.config.maximum_residual_rms_dbfs << ",\n"
          << "    \"maximum_residual_peak_dbfs\": " << result.config.maximum_residual_peak_dbfs << "\n"
          << "  },\n"
          << "  \"sample_rate\": " << result.sample_rate << ",\n"
          << "  \"capture_delay_frames\": " << result.capture_delay_frames << ",\n"
          << "  \"capture_delay_ms\": " << result.capture_delay_ms << ",\n"
          << "  \"compared_frames\": " << result.compared_frames << ",\n"
          << "  \"applied_capture_gain_db\": " << result.applied_capture_gain_db << ",\n"
          << "  \"spectral_rms_magnitude_error_db\": " << result.spectral_rms_magnitude_error_db << ",\n"
          << "  \"spectral_worst_magnitude_error_db\": " << result.spectral_worst_magnitude_error_db << ",\n"
          << "  \"spectral_worst_phase_error_degrees\": " << result.spectral_worst_phase_error_degrees << ",\n";

        auto channel =
            [&o](
                const char* name,
                const ValidationChannelMetrics& m,
                bool comma
            ) {
                o << "  \"" << name << "\": {\n"
                  << "    \"reference_rms_dbfs\": " << m.reference_rms_dbfs << ",\n"
                  << "    \"capture_rms_dbfs\": " << m.capture_rms_dbfs << ",\n"
                  << "    \"gain_error_db\": " << m.gain_error_db << ",\n"
                  << "    \"correlation\": " << m.correlation << ",\n"
                  << "    \"residual_rms_dbfs\": " << m.residual_rms_dbfs << ",\n"
                  << "    \"residual_peak_dbfs\": " << m.residual_peak_dbfs << ",\n"
                  << "    \"snr_db\": " << m.snr_db << "\n"
                  << "  }" << (comma ? "," : "") << "\n";
            };

        channel("left", result.left, true);
        channel("right", result.right, true);

        o << "  \"failures\": [";
        for (std::size_t i = 0; i < result.failures.size(); ++i) {
            if (i) o << ", ";
            o << "\"" << json_escape(result.failures[i]) << "\"";
        }
        o << "]\n}\n";
        o.flush();
        if (!o)
            return fail(
                error,
                "failed while writing report staging file for "
                    + path_utf8(json_path));
    }

    {
        std::ofstream o(md_stage);
        if (!o)
            return fail(
                error,
                "cannot create report staging file for "
                    + path_utf8(md_path));

        o << "# Spin FV-1 Validation Report\n\n"
          << "**Result:** " << (result.passed ? "PASS" : "FAIL") << "\n\n"
          << "- Sample rate: " << result.sample_rate << " Hz\n"
          << "- Compared frames: " << result.compared_frames << "\n"
          << "- Capture delay: " << result.capture_delay_frames << " frames ("
          << std::fixed << std::setprecision(4) << result.capture_delay_ms << " ms)\n"
          << "- Residual gain correction: " << result.applied_capture_gain_db << " dB\n"
          << "- Spectral magnitude error RMS/worst: " << result.spectral_rms_magnitude_error_db
          << " / " << result.spectral_worst_magnitude_error_db << " dB\n"
          << "- Worst spectral phase error: " << result.spectral_worst_phase_error_degrees << " degrees\n\n"
          << "## Acceptance configuration\n\n"
          << "- Maximum alignment search: " << result.config.max_alignment_ms << " ms\n"
          << "- Gain-match residual: " << (result.config.gain_match_residual ? "yes" : "no") << "\n"
          << "- Spectral FFT: " << result.config.fft_size << "\n"
          << "- Minimum correlation: " << result.config.minimum_correlation << "\n"
          << "- Maximum residual RMS: " << result.config.maximum_residual_rms_dbfs << " dBFS\n"
          << "- Maximum residual peak: " << result.config.maximum_residual_peak_dbfs << " dBFS\n\n"
          << "| Metric | Left | Right |\n|---|---:|---:|\n"
          << "| Correlation | " << result.left.correlation << " | " << result.right.correlation << " |\n"
          << "| Gain error (dB) | " << result.left.gain_error_db << " | " << result.right.gain_error_db << " |\n"
          << "| Residual RMS (dBFS) | " << result.left.residual_rms_dbfs << " | " << result.right.residual_rms_dbfs << " |\n"
          << "| Residual peak (dBFS) | " << result.left.residual_peak_dbfs << " | " << result.right.residual_peak_dbfs << " |\n"
          << "| SNR (dB) | " << result.left.snr_db << " | " << result.right.snr_db << " |\n\n";

        if (!result.failures.empty()) {
            o << "## Failed limits\n\n";
            for (const auto& f : result.failures) o << "- " << f << "\n";
            o << '\n';
        }

        o << "Frequency-response data: `" << path_utf8(csv_path.filename()) << "`  \n"
          << "Residual audio: `" << path_utf8(wav_path.filename()) << "`\n";
        o.flush();
        if (!o)
            return fail(
                error,
                "failed while writing report staging file for "
                    + path_utf8(md_path));
    }

    {
        std::ofstream o(csv_stage);
        if (!o)
            return fail(
                error,
                "cannot create report staging file for "
                    + path_utf8(csv_path));
        o << "frequency_hz,magnitude_error_db,phase_error_degrees,reference_level_dbfs\n";
        o << std::setprecision(10);
        for (const auto& p : result.frequency_response)
            o << p.frequency_hz << ',' << p.magnitude_error_db << ','
              << p.phase_error_degrees << ',' << p.reference_level_dbfs << '\n';
        o.flush();
        if (!o)
            return fail(
                error,
                "failed while writing report staging file for "
                    + path_utf8(csv_path));
    }

    ValidationAudio residual{result.sample_rate, result.residual};
    if (!write_validation_wav(wav_stage, residual, error))
        return false;

    return transaction.commit(error);
}

bool generate_validation_stimulus(ValidationAudio& audio,
                                  std::uint32_t sample_rate,
                                  double seconds,
                                  const std::string& kind,
                                  double level,
                                  double frequency_hz,
                                  double sweep_end_hz,
                                  std::uint32_t seed,
                                  std::string* error) {
    if (sample_rate < 8000 || sample_rate > 384000) return fail(error, "unsupported sample rate");
    if (!(seconds > 0.0) || seconds > 600.0) return fail(error, "duration must be > 0 and <= 600 seconds");
    if (!(level > 0.0) || level > 0.95) return fail(error, "level must be > 0 and <= 0.95");
    const std::size_t frames = static_cast<std::size_t>(std::llround(seconds * static_cast<double>(sample_rate)));
    ValidationAudio out;
    out.sample_rate = sample_rate;
    out.frames.resize(frames);
    std::uint32_t rng = seed ? seed : 1u;
    double pink0 = 0.0, pink1 = 0.0, pink2 = 0.0;
    auto rnd = [&]() {
        rng = rng * 1664525u + 1013904223u;
        return static_cast<double>(static_cast<std::int32_t>(rng)) / 2147483648.0;
    };
    const double f0 = std::clamp(frequency_hz, 1.0, 0.45 * static_cast<double>(sample_rate));
    const double f1 = std::clamp(sweep_end_hz, f0, 0.45 * static_cast<double>(sample_rate));
    const std::array<double, 12> multitone{31.0, 63.0, 125.0, 250.0, 500.0, 1000.0,
                                           2000.0, 4000.0, 6000.0, 8000.0, 12000.0, 16000.0};
    for (std::size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(sample_rate);
        double x = 0.0;
        if (kind == "sine") {
            x = level * std::sin(2.0 * kPi * f0 * t);
        } else if (kind == "sweep") {
            const double k = std::log(f1 / f0) / seconds;
            if (std::abs(k) < 1.0e-15) {
                x = level * std::sin(2.0 * kPi * f0 * t);
            } else {
                const double phase = 2.0 * kPi * f0 * (std::exp(k * t) - 1.0) / k;
                x = level * std::sin(phase);
            }
        } else if (kind == "white") {
            x = level * rnd();
        } else if (kind == "pink") {
            const double w = rnd();
            pink0 = 0.99765 * pink0 + w * 0.0990460;
            pink1 = 0.96300 * pink1 + w * 0.2965164;
            pink2 = 0.57000 * pink2 + w * 1.0526913;
            x = level * std::clamp((pink0 + pink1 + pink2 + w * 0.1848) * 0.12, -1.0, 1.0);
        } else if (kind == "impulse") {
            x = (i % sample_rate == 0) ? level : 0.0;
        } else if (kind == "multitone") {
            double sum = 0.0;
            std::size_t used = 0;
            for (double f : multitone) {
                if (f < 0.45 * static_cast<double>(sample_rate)) {
                    sum += std::sin(2.0 * kPi * f * t + 0.17 * static_cast<double>(used));
                    ++used;
                }
            }
            x = used ? level * sum / static_cast<double>(used) : 0.0;
        } else {
            return fail(error, "unknown stimulus kind: " + kind);
        }
        out.frames[i] = {static_cast<float>(x), static_cast<float>(x)};
    }
    audio = std::move(out);
    return true;
}


bool write_validation_stimulus_pack(const std::filesystem::path& directory,
                                    const ValidationPackConfig& config,
                                    std::string* error) {
    if (config.sample_rate < 8000 || config.sample_rate > 384000)
        return fail(error, "validation pack sample rate must be 8000..384000 Hz");
    if (!(config.standard_seconds > 0.0) || config.standard_seconds > 120.0)
        return fail(error, "validation pack duration must be > 0 and <= 120 seconds");
    if (!(config.level > 0.0) || config.level > 0.8)
        return fail(error, "validation pack level must be > 0 and <= 0.8");

    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) return fail(error, "cannot create validation-pack directory: " + ec.message());

    struct Stimulus {
        const char* filename;
        const char* kind;
        double seconds_scale;
        double level_scale;
        double frequency_hz;
        double sweep_end_hz;
    };
    const std::array<Stimulus, 6> stimuli{{
        {"01-impulse.wav", "impulse", 0.4, 1.0, 440.0, 16000.0},
        {"02-multitone.wav", "multitone", 1.0, 1.0, 440.0, 16000.0},
        {"03-log-sweep.wav", "sweep", 2.0, 1.0, 20.0, 20000.0},
        {"04-sine-1khz.wav", "sine", 1.0, 1.0, 1000.0, 1000.0},
        {"05-white-noise.wav", "white", 1.0, 0.72, 440.0, 16000.0},
        {"06-pink-noise.wav", "pink", 1.0, 0.72, 440.0, 16000.0}
    }};

    for (std::size_t i = 0; i < stimuli.size(); ++i) {
        const auto& spec = stimuli[i];
        ValidationAudio audio;
        const double seconds = std::max(0.05, config.standard_seconds * spec.seconds_scale);
        const double level = config.level * spec.level_scale;
        const double sweep_end = std::min(spec.sweep_end_hz, 0.45 * static_cast<double>(config.sample_rate));
        if (!generate_validation_stimulus(audio, config.sample_rate, seconds, spec.kind,
                                          level, spec.frequency_hz, sweep_end,
                                          config.seed + static_cast<std::uint32_t>(i), error))
            return false;
        if (!write_validation_wav(directory / spec.filename, audio, error)) return false;
    }

    const auto manifest_path = directory / "manifest.json";
    const auto manifest_stage = make_temp_sibling(manifest_path);
    remove_quietly(manifest_stage);
    std::ofstream manifest(manifest_stage);
    if (!manifest)
        return fail(error, "cannot create " + path_utf8(manifest_path));
    manifest << std::fixed << std::setprecision(6);
    manifest << "{\n"
             << "  \"schema\": \"spin-fv1-hardware-validation-pack-1\",\n"
             << "  \"sample_rate\": " << config.sample_rate << ",\n"
             << "  \"standard_seconds\": " << config.standard_seconds << ",\n"
             << "  \"level\": " << config.level << ",\n"
             << "  \"seed\": " << config.seed << ",\n"
             << "  \"workflow\": \"Feed each WAV unchanged to both the emulator and physical FV-1 fixture; capture hardware output at the same host sample rate; validate reference render vs capture.\",\n"
             << "  \"stimuli\": [\n";
    for (std::size_t i = 0; i < stimuli.size(); ++i) {
        const auto& spec = stimuli[i];
        const double seconds = std::max(0.05, config.standard_seconds * spec.seconds_scale);
        const double level = config.level * spec.level_scale;
        const double sweep_end = std::min(spec.sweep_end_hz, 0.45 * static_cast<double>(config.sample_rate));
        manifest << "    {\"file\": \"" << spec.filename << "\", \"kind\": \"" << spec.kind
                 << "\", \"seconds\": " << seconds << ", \"level\": " << level
                 << ", \"frequency_hz\": " << spec.frequency_hz
                 << ", \"sweep_end_hz\": " << sweep_end
                 << ", \"seed\": " << (config.seed + static_cast<std::uint32_t>(i)) << "}"
                 << (i + 1 == stimuli.size() ? "\n" : ",\n");
    }
    manifest << "  ]\n}\n";
    manifest.flush();
    const bool manifest_ok = static_cast<bool>(manifest);
    manifest.close();
    if (!manifest_ok) {
        remove_quietly(manifest_stage);
        return fail(error, "failed while writing " + path_utf8(manifest_path));
    }
    if (!replace_completed_file(manifest_stage, manifest_path, error)) {
        remove_quietly(manifest_stage);
        return false;
    }

    const auto readme_path = directory / "README.txt";
    const auto readme_stage = make_temp_sibling(readme_path);
    remove_quietly(readme_stage);
    std::ofstream readme(readme_stage);
    if (!readme)
        return fail(error, "cannot create " + path_utf8(readme_path));
    readme << "Spin FV-1 Emulator — Phase 5B Hardware Validation Pack\n\n"
           << "1. Use the same FV-1 program, clock and POT settings for virtual and physical runs.\n"
           << "2. Render each stimulus through the emulator to create reference output.\n"
           << "3. Play the untouched stimulus through the physical FV-1 fixture and record its output.\n"
           << "4. Capture at " << config.sample_rate << " Hz without normalization, limiting or post-processing.\n"
           << "5. Compare each reference/capture pair with fv1-cli validate or the VALIDATION tab.\n"
           << "6. Preserve the manifest with the captures so regressions remain reproducible.\n";

    readme.flush();
    const bool readme_ok = static_cast<bool>(readme);
    readme.close();
    if (!readme_ok) {
        remove_quietly(readme_stage);
        return fail(error, "failed while writing " + path_utf8(readme_path));
    }
    if (!replace_completed_file(readme_stage, readme_path, error)) {
        remove_quietly(readme_stage);
        return false;
    }
    return true;
}

} // namespace fv1
