#include <fv1/analysis.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace fv1 {
namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;

bool is_power_of_two(std::size_t n) noexcept { return n >= 2 && (n & (n - 1)) == 0; }

class SpscFrameRing {
public:
    void prepare(std::size_t requested) {
        std::size_t cap = 2;
        while (cap < requested) cap <<= 1;
        data_.assign(cap, {});
        mask_ = cap - 1;
        read_.store(0, std::memory_order_relaxed);
        write_.store(0, std::memory_order_relaxed);
    }

    std::size_t push(const StereoFrame* src, std::size_t count) noexcept {
        if (!src || data_.empty()) return 0;
        std::size_t w = write_.load(std::memory_order_relaxed);
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
        std::size_t r = read_.load(std::memory_order_relaxed);
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

void fft_in_place(std::vector<std::complex<float>>& a) {
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const float angle = static_cast<float>(-2.0 * kPi / static_cast<double>(len));
        const std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
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

} // namespace

class AnalyzerWorker::Impl {
public:
    double sample_rate{48000.0};
    std::size_t fft_size{1024};
    SpscFrameRing queue;
    std::vector<StereoFrame> block;
    std::vector<std::complex<float>> fft;
    std::vector<float> window;
    mutable std::mutex snapshot_mutex;
    AnalysisSnapshot snapshot;
    std::thread worker;
    std::atomic<bool> running{false};
    std::atomic<std::uint64_t> dropped{0};
    std::atomic<std::uint64_t> sequence{0};

    void analyze_block() {
        float peak_l = 0.0f, peak_r = 0.0f;
        double sum_l2 = 0.0, sum_r2 = 0.0, sum_lr = 0.0;
        for (std::size_t i = 0; i < fft_size; ++i) {
            const float l = block[i].left;
            const float r = block[i].right;
            peak_l = std::max(peak_l, std::abs(l));
            peak_r = std::max(peak_r, std::abs(r));
            sum_l2 += static_cast<double>(l) * l;
            sum_r2 += static_cast<double>(r) * r;
            sum_lr += static_cast<double>(l) * r;
            const float mono = 0.5f * (l + r);
            fft[i] = {mono * window[i], 0.0f};
        }
        fft_in_place(fft);

        AnalysisSnapshot next;
        next.sample_rate = sample_rate;
        next.sequence = sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        next.peak_left = peak_l;
        next.peak_right = peak_r;
        next.rms_left = static_cast<float>(std::sqrt(sum_l2 / static_cast<double>(fft_size)));
        next.rms_right = static_cast<float>(std::sqrt(sum_r2 / static_cast<double>(fft_size)));
        const double denom = std::sqrt(sum_l2 * sum_r2);
        next.correlation = denom > 1e-20 ? static_cast<float>(std::clamp(sum_lr / denom, -1.0, 1.0)) : 0.0f;

        const std::size_t bins = fft_size / 2 + 1;
        next.spectrum_db.resize(bins);
        float dominant_db = -200.0f;
        std::size_t dominant_bin = 0;
        const float scale = 2.0f / static_cast<float>(fft_size);
        for (std::size_t bin = 0; bin < bins; ++bin) {
            const float mag = std::abs(fft[bin]) * scale;
            const float db = 20.0f * std::log10(std::max(mag, 1.0e-10f));
            next.spectrum_db[bin] = db;
            if (bin > 0 && db > dominant_db) { dominant_db = db; dominant_bin = bin; }
        }
        next.dominant_level_db = dominant_db;
        next.dominant_frequency_hz = static_cast<float>(
            static_cast<double>(dominant_bin) * sample_rate / static_cast<double>(fft_size));

        std::lock_guard lock(snapshot_mutex);
        snapshot = std::move(next);
    }

    void loop() {
        while (running.load(std::memory_order_acquire)) {
            if (queue.available() < fft_size) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            if (queue.pop(block.data(), fft_size) == fft_size) analyze_block();
        }
    }
};

AnalyzerWorker::AnalyzerWorker() : impl_(std::make_unique<Impl>()) {}
AnalyzerWorker::~AnalyzerWorker() { stop(); }

bool AnalyzerWorker::prepare(double sample_rate, std::size_t fft_size, std::size_t queue_frames) {
    stop();
    if (!(sample_rate > 1000.0) || !is_power_of_two(fft_size) || queue_frames < fft_size * 2)
        return false;
    impl_->sample_rate = sample_rate;
    impl_->fft_size = fft_size;
    impl_->queue.prepare(queue_frames);
    impl_->block.assign(fft_size, {});
    impl_->fft.assign(fft_size, {});
    impl_->window.resize(fft_size);
    for (std::size_t i = 0; i < fft_size; ++i) {
        impl_->window[i] = static_cast<float>(0.5 - 0.5 * std::cos(
            2.0 * kPi * static_cast<double>(i) / static_cast<double>(fft_size - 1)));
    }
    impl_->dropped.store(0, std::memory_order_relaxed);
    impl_->sequence.store(0, std::memory_order_relaxed);
    {
        std::lock_guard lock(impl_->snapshot_mutex);
        impl_->snapshot = {};
        impl_->snapshot.sample_rate = sample_rate;
        impl_->snapshot.spectrum_db.assign(fft_size / 2 + 1, -200.0f);
    }
    return true;
}

void AnalyzerWorker::start() {
    if (impl_->running.exchange(true, std::memory_order_acq_rel)) return;
    impl_->worker = std::thread([this] { impl_->loop(); });
}
void AnalyzerWorker::stop() {
    if (!impl_->running.exchange(false, std::memory_order_acq_rel)) return;
    if (impl_->worker.joinable()) impl_->worker.join();
}
void AnalyzerWorker::push(const StereoFrame* frames, std::size_t count) noexcept {
    if (!frames || count == 0) return;
    const std::size_t written = impl_->queue.push(frames, count);
    if (written < count) impl_->dropped.fetch_add(count - written, std::memory_order_relaxed);
}
AnalysisSnapshot AnalyzerWorker::latest() const {
    std::lock_guard lock(impl_->snapshot_mutex);
    return impl_->snapshot;
}
std::uint64_t AnalyzerWorker::dropped_frames() const noexcept {
    return impl_->dropped.load(std::memory_order_relaxed);
}

} // namespace fv1
