#include <fv1/audio_recorder.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace fv1 {
namespace {

class SpscFrameRing {
public:
    void prepare(std::size_t requested) {
        std::size_t capacity = 2;
        while (capacity < requested) capacity <<= 1;
        data_.assign(capacity, {});
        mask_ = capacity - 1;
        read_.store(0, std::memory_order_relaxed);
        write_.store(0, std::memory_order_relaxed);
    }

    void clear() noexcept {
        const auto w = write_.load(std::memory_order_acquire);
        read_.store(w, std::memory_order_release);
    }

    std::size_t push(const StereoFrame* src, std::size_t count) noexcept {
        if (!src || data_.empty()) return 0;
        const std::size_t w = write_.load(std::memory_order_relaxed);
        const std::size_t r = read_.load(std::memory_order_acquire);
        const std::size_t capacity = data_.size();
        const std::size_t used = w - r;
        const std::size_t free = capacity - std::min(capacity, used);
        const std::size_t n = std::min(count, free);
        for (std::size_t i = 0; i < n; ++i) data_[(w + i) & mask_] = src[i];
        write_.store(w + n, std::memory_order_release);
        return n;
    }

    std::size_t pop(StereoFrame* dst, std::size_t count) noexcept {
        if (!dst || data_.empty()) return 0;
        const std::size_t r = read_.load(std::memory_order_relaxed);
        const std::size_t w = write_.load(std::memory_order_acquire);
        const std::size_t n = std::min(count, w - r);
        for (std::size_t i = 0; i < n; ++i) dst[i] = data_[(r + i) & mask_];
        read_.store(r + n, std::memory_order_release);
        return n;
    }

    std::size_t available() const noexcept {
        return write_.load(std::memory_order_acquire) - read_.load(std::memory_order_acquire);
    }

private:
    std::vector<StereoFrame> data_;
    std::size_t mask_{};
    alignas(64) std::atomic<std::size_t> read_{0};
    alignas(64) std::atomic<std::size_t> write_{0};
};

void write_u16(std::ostream& out, std::uint16_t value) {
    const std::array<char, 2> bytes{
        static_cast<char>(value & 0xffu),
        static_cast<char>((value >> 8u) & 0xffu)};
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void write_u32(std::ostream& out, std::uint32_t value) {
    const std::array<char, 4> bytes{
        static_cast<char>(value & 0xffu),
        static_cast<char>((value >> 8u) & 0xffu),
        static_cast<char>((value >> 16u) & 0xffu),
        static_cast<char>((value >> 24u) & 0xffu)};
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

class FloatWavWriter {
public:
    bool open(const std::filesystem::path& path, std::uint32_t sample_rate, std::string* error) {
        close();
        path_ = path;
        stream_.open(path, std::ios::binary | std::ios::trunc);
        if (!stream_) {
            if (error) *error = "could not create WAV file: " + path.string();
            return false;
        }
        sample_rate_ = sample_rate;
        frames_written_ = 0;
        write_header(0);
        if (!stream_) {
            if (error) *error = "could not write WAV header: " + path.string();
            close();
            return false;
        }
        return true;
    }

    void write(const StereoFrame* frames, std::size_t count) {
        if (!stream_ || !frames || count == 0) return;
        static_assert(sizeof(StereoFrame) == sizeof(float) * 2,
                      "StereoFrame must be two packed float values for WAV output");
        stream_.write(reinterpret_cast<const char*>(frames),
                      static_cast<std::streamsize>(count * sizeof(StereoFrame)));
        if (stream_) frames_written_ += count;
    }

    void close() noexcept {
        if (!stream_.is_open()) return;
        try {
            stream_.flush();
            stream_.seekp(0, std::ios::beg);
            write_header(frames_written_);
            stream_.flush();
            stream_.close();
        } catch (...) {
            // Destructors/stop paths must stay noexcept. Any I/O error simply
            // leaves the best-effort capture file on disk.
            try { stream_.close(); } catch (...) {}
        }
    }

    std::uint64_t frames_written() const noexcept { return frames_written_; }

private:
    void write_header(std::uint64_t frames) {
        constexpr std::uint16_t channels = 2;
        constexpr std::uint16_t bits_per_sample = 32;
        constexpr std::uint16_t format_ieee_float = 3;
        constexpr std::uint16_t block_align = channels * bits_per_sample / 8;
        const std::uint64_t data_bytes64 = frames * block_align;
        const std::uint32_t data_bytes = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(data_bytes64, 0xffffffffu - 36u));
        const std::uint32_t byte_rate = sample_rate_ * block_align;

        stream_.write("RIFF", 4);
        write_u32(stream_, 36u + data_bytes);
        stream_.write("WAVE", 4);
        stream_.write("fmt ", 4);
        write_u32(stream_, 16u);
        write_u16(stream_, format_ieee_float);
        write_u16(stream_, channels);
        write_u32(stream_, sample_rate_);
        write_u32(stream_, byte_rate);
        write_u16(stream_, block_align);
        write_u16(stream_, bits_per_sample);
        stream_.write("data", 4);
        write_u32(stream_, data_bytes);
    }

    std::filesystem::path path_;
    std::ofstream stream_;
    std::uint32_t sample_rate_{};
    std::uint64_t frames_written_{};
};

std::filesystem::path with_suffix(const std::filesystem::path& base, const std::string& suffix) {
    auto parent = base.parent_path();
    auto stem = base.stem().string();
    auto ext = base.extension().string();
    if (ext.empty()) ext = ".wav";
    return parent / (stem + suffix + ext);
}

} // namespace

class AudioRecorder::Impl {
public:
    AudioRecordMode mode{AudioRecordMode::Processed};
    std::uint32_t sample_rate{};
    std::filesystem::path raw_path;
    std::filesystem::path processed_path;
    SpscFrameRing raw_ring;
    SpscFrameRing processed_ring;
    FloatWavWriter raw_writer;
    FloatWavWriter processed_writer;
    std::vector<StereoFrame> scratch;
    std::thread worker;
    std::atomic<bool> prepared{false};
    std::atomic<bool> running{false};
    std::atomic<std::uint64_t> raw_written{0};
    std::atomic<std::uint64_t> processed_written{0};
    std::atomic<std::uint64_t> raw_dropped{0};
    std::atomic<std::uint64_t> processed_dropped{0};

    bool wants_raw() const noexcept {
        return mode == AudioRecordMode::Raw || mode == AudioRecordMode::RawAndProcessed;
    }
    bool wants_processed() const noexcept {
        return mode == AudioRecordMode::Processed || mode == AudioRecordMode::RawAndProcessed;
    }

    void drain_once() {
        if (wants_raw()) {
            const auto n = raw_ring.pop(scratch.data(), scratch.size());
            if (n) {
                raw_writer.write(scratch.data(), n);
                raw_written.fetch_add(n, std::memory_order_relaxed);
            }
        }
        if (wants_processed()) {
            const auto n = processed_ring.pop(scratch.data(), scratch.size());
            if (n) {
                processed_writer.write(scratch.data(), n);
                processed_written.fetch_add(n, std::memory_order_relaxed);
            }
        }
    }

    bool queues_empty() const noexcept {
        const bool raw_empty = !wants_raw() || raw_ring.available() == 0;
        const bool processed_empty = !wants_processed() || processed_ring.available() == 0;
        return raw_empty && processed_empty;
    }

    void loop() {
        while (running.load(std::memory_order_acquire) || !queues_empty()) {
            const bool had_data = !queues_empty();
            drain_once();
            if (!had_data) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    void close_writers() noexcept {
        raw_writer.close();
        processed_writer.close();
    }
};

AudioRecorder::AudioRecorder() : impl_(std::make_unique<Impl>()) {}
AudioRecorder::~AudioRecorder() { stop(); }

bool AudioRecorder::prepare(const std::filesystem::path& base_path,
                            std::uint32_t sample_rate,
                            AudioRecordMode mode,
                            std::size_t queue_frames,
                            std::string* error) {
    stop();
    impl_->prepared.store(false, std::memory_order_release);
    if (sample_rate < 8000 || sample_rate > 768000 || queue_frames < 1024 || base_path.empty()) {
        if (error) *error = "invalid recorder sample rate, queue size, or path";
        return false;
    }

    impl_->mode = mode;
    impl_->sample_rate = sample_rate;
    impl_->raw_path.clear();
    impl_->processed_path.clear();
    impl_->raw_written.store(0, std::memory_order_relaxed);
    impl_->processed_written.store(0, std::memory_order_relaxed);
    impl_->raw_dropped.store(0, std::memory_order_relaxed);
    impl_->processed_dropped.store(0, std::memory_order_relaxed);
    impl_->raw_ring.prepare(queue_frames);
    impl_->processed_ring.prepare(queue_frames);
    impl_->scratch.assign(std::min<std::size_t>(queue_frames / 4, 16384), {});

    if (mode == AudioRecordMode::RawAndProcessed) {
        impl_->raw_path = with_suffix(base_path, "-raw");
        impl_->processed_path = with_suffix(base_path, "-processed");
    } else if (mode == AudioRecordMode::Raw) {
        impl_->raw_path = base_path.extension().empty() ? std::filesystem::path(base_path.string() + ".wav") : base_path;
    } else {
        impl_->processed_path = base_path.extension().empty() ? std::filesystem::path(base_path.string() + ".wav") : base_path;
    }

    std::error_code ec;
    const auto parent = base_path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    if (ec) {
        if (error) *error = "could not create capture directory: " + ec.message();
        return false;
    }

    std::string local_error;
    if (impl_->wants_raw() && !impl_->raw_writer.open(impl_->raw_path, sample_rate, &local_error)) {
        if (error) *error = local_error;
        impl_->close_writers();
        return false;
    }
    if (impl_->wants_processed() && !impl_->processed_writer.open(impl_->processed_path, sample_rate, &local_error)) {
        if (error) *error = local_error;
        impl_->close_writers();
        return false;
    }

    impl_->prepared.store(true, std::memory_order_release);
    return true;
}

bool AudioRecorder::start(std::string* error) {
    if (!impl_->prepared.load(std::memory_order_acquire)) {
        if (error) *error = "recorder is not prepared";
        return false;
    }
    if (impl_->running.exchange(true, std::memory_order_acq_rel)) return true;
    try {
        impl_->worker = std::thread([this]{ impl_->loop(); });
    } catch (const std::exception& ex) {
        impl_->running.store(false, std::memory_order_release);
        if (error) *error = std::string("could not start recorder worker: ") + ex.what();
        return false;
    }
    return true;
}

void AudioRecorder::stop() noexcept {
    const bool was_running = impl_->running.exchange(false, std::memory_order_acq_rel);
    if (was_running && impl_->worker.joinable()) impl_->worker.join();
    else if (impl_->worker.joinable()) impl_->worker.join();
    if (impl_->prepared.exchange(false, std::memory_order_acq_rel)) {
        while (!impl_->queues_empty()) impl_->drain_once();
        impl_->close_writers();
    }
}

bool AudioRecorder::prepared() const noexcept { return impl_->prepared.load(std::memory_order_acquire); }
bool AudioRecorder::recording() const noexcept { return impl_->running.load(std::memory_order_acquire); }
AudioRecordMode AudioRecorder::mode() const noexcept { return impl_->mode; }

void AudioRecorder::push_raw(const StereoFrame* frames, std::size_t count) noexcept {
    if (!recording() || !impl_->wants_raw()) return;
    const auto n = impl_->raw_ring.push(frames, count);
    if (n < count) impl_->raw_dropped.fetch_add(count - n, std::memory_order_relaxed);
}

void AudioRecorder::push_processed(const StereoFrame* frames, std::size_t count) noexcept {
    if (!recording() || !impl_->wants_processed()) return;
    const auto n = impl_->processed_ring.push(frames, count);
    if (n < count) impl_->processed_dropped.fetch_add(count - n, std::memory_order_relaxed);
}

AudioRecorderStats AudioRecorder::stats() const noexcept {
    return {
        impl_->raw_written.load(std::memory_order_relaxed),
        impl_->processed_written.load(std::memory_order_relaxed),
        impl_->raw_dropped.load(std::memory_order_relaxed),
        impl_->processed_dropped.load(std::memory_order_relaxed)};
}

std::filesystem::path AudioRecorder::raw_path() const { return impl_->raw_path; }
std::filesystem::path AudioRecorder::processed_path() const { return impl_->processed_path; }

} // namespace fv1
