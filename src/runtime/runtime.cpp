#include <fv1/runtime.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <numeric>
#include <vector>

#if defined(FV1_HAVE_SPEEXDSP)
#include <speex/speex_resampler.h>
#endif

namespace fv1 {
namespace {

class FrameRing {
public:
    void prepare(std::size_t capacity) {
        capacity_ = std::max<std::size_t>(capacity, 8);
        data_.assign(capacity_, {});
        read_ = write_ = size_ = 0;
    }
    void clear() noexcept { read_ = write_ = size_ = 0; }
    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return capacity_; }

    std::size_t push(const StereoFrame* src, std::size_t count) noexcept {
        const std::size_t writable = std::min(count, capacity_ - size_);
        for (std::size_t i = 0; i < writable; ++i) {
            data_[write_] = src[i];
            write_ = (write_ + 1) % capacity_;
        }
        size_ += writable;
        return writable;
    }
    std::size_t pop(StereoFrame* dst, std::size_t count) noexcept {
        const std::size_t readable = std::min(count, size_);
        for (std::size_t i = 0; i < readable; ++i) {
            dst[i] = data_[read_];
            read_ = (read_ + 1) % capacity_;
        }
        size_ -= readable;
        return readable;
    }
private:
    std::vector<StereoFrame> data_;
    std::size_t capacity_{};
    std::size_t read_{};
    std::size_t write_{};
    std::size_t size_{};
};

/* Small streaming linear SRC used as a build/runtime fallback when SpeexDSP
   is unavailable. Production Linux bootstrap installs SpeexDSP, so normal
   builds use the higher-quality implementation below. */
class LinearStereoResampler {
public:
    bool init(double input_rate, double output_rate) {
        if (!(input_rate > 0.0) || !(output_rate > 0.0)) return false;
        step_ = input_rate / output_rate;
        phase_ = 0.0;
        have_prev_ = false;
        prev_ = {};
        return true;
    }
    void reset() noexcept { phase_ = 0.0; have_prev_ = false; prev_ = {}; }

    std::size_t process(const StereoFrame* input, std::size_t input_frames,
                        StereoFrame* output, std::size_t output_capacity,
                        std::size_t* consumed) noexcept {
        if (consumed) *consumed = input_frames;
        if (!input || input_frames == 0 || !output || output_capacity == 0) return 0;
        if (!have_prev_) { prev_ = input[0]; have_prev_ = true; }

        std::size_t produced = 0;
        /* phase is measured in current-block input-frame coordinates. Negative
           values interpolate from prev_ to input[0] after block rollover. */
        while (produced < output_capacity) {
            const long i0 = static_cast<long>(std::floor(phase_));
            const double frac = phase_ - static_cast<double>(i0);
            if (i0 >= static_cast<long>(input_frames)) break;

            StereoFrame a{}, b{};
            if (i0 < 0) {
                a = prev_;
                b = input[0];
            } else {
                a = input[static_cast<std::size_t>(i0)];
                const std::size_t next = static_cast<std::size_t>(i0) + 1;
                if (next >= input_frames) {
                    /* Need a sample from the next block for interpolation. */
                    if (frac > 1e-12) break;
                    b = a;
                } else {
                    b = input[next];
                }
            }
            output[produced++] = {
                static_cast<float>(a.left + (b.left - a.left) * frac),
                static_cast<float>(a.right + (b.right - a.right) * frac)
            };
            phase_ += step_;
        }
        prev_ = input[input_frames - 1];
        phase_ -= static_cast<double>(input_frames);
        return produced;
    }
private:
    double step_{1.0};
    double phase_{};
    bool have_prev_{};
    StereoFrame prev_{};
};

class StereoResampler {
public:
    ~StereoResampler() { destroy(); }
    bool init(double input_rate, double output_rate, int quality) {
        destroy();
#if !defined(FV1_HAVE_SPEEXDSP)
        (void)quality;
#endif
        in_rate_ = input_rate;
        out_rate_ = output_rate;
        bypass_ = std::abs(input_rate - output_rate) < 0.01;
        if (bypass_) return true;
#if defined(FV1_HAVE_SPEEXDSP)
        /* speex_resampler_init() accepts integer-Hz rates only.  Real FV-1
           crystal-derived rates are not necessarily integral (46.6084 kHz is
           an important example), so use SpeexDSP's fractional-rate entry
           point.  A 1 mHz rational grid keeps normal audio rates well inside
           32-bit numerator/denominator limits while preserving practical
           crystal-rate precision. */
        constexpr double kRateScale = 1000.0;
        const auto in_scaled64 = static_cast<std::uint64_t>(std::llround(input_rate * kRateScale));
        const auto out_scaled64 = static_cast<std::uint64_t>(std::llround(output_rate * kRateScale));
        if (in_scaled64 > 0 && out_scaled64 > 0 &&
            in_scaled64 <= UINT32_MAX && out_scaled64 <= UINT32_MAX) {
            const std::uint64_t divisor = std::gcd(in_scaled64, out_scaled64);
            const auto ratio_num = static_cast<spx_uint32_t>(in_scaled64 / divisor);
            const auto ratio_den = static_cast<spx_uint32_t>(out_scaled64 / divisor);
            int err = RESAMPLER_ERR_SUCCESS;
            state_ = speex_resampler_init_frac(
                2, ratio_num, ratio_den,
                static_cast<spx_uint32_t>(std::llround(input_rate)),
                static_cast<spx_uint32_t>(std::llround(output_rate)),
                std::clamp(quality, 0, 10), &err);
            use_speex_ = state_ != nullptr && err == RESAMPLER_ERR_SUCCESS;
            if (use_speex_) return true;
        }
#endif
        use_speex_ = false;
        return linear_.init(input_rate, output_rate);
    }
    void reset() noexcept {
#if defined(FV1_HAVE_SPEEXDSP)
        if (state_) speex_resampler_reset_mem(state_);
#endif
        linear_.reset();
    }
    bool using_speex() const noexcept { return !bypass_ && use_speex_; }

    std::size_t process(const StereoFrame* input, std::size_t input_frames,
                        StereoFrame* output, std::size_t output_capacity,
                        std::size_t* consumed) noexcept {
        if (consumed) *consumed = 0;
        if (bypass_) {
            const std::size_t n = std::min(input_frames, output_capacity);
            if (n) std::memcpy(output, input, n * sizeof(StereoFrame));
            if (consumed) *consumed = n;
            return n;
        }
#if defined(FV1_HAVE_SPEEXDSP)
        if (use_speex_ && state_) {
            spx_uint32_t in_len = static_cast<spx_uint32_t>(std::min<std::size_t>(input_frames, UINT32_MAX));
            spx_uint32_t out_len = static_cast<spx_uint32_t>(std::min<std::size_t>(output_capacity, UINT32_MAX));
            const int rc = speex_resampler_process_interleaved_float(
                state_, reinterpret_cast<const float*>(input), &in_len,
                reinterpret_cast<float*>(output), &out_len);
            if (rc != RESAMPLER_ERR_SUCCESS) return 0;
            if (consumed) *consumed = in_len;
            return out_len;
        }
#endif
        return linear_.process(input, input_frames, output, output_capacity, consumed);
    }
private:
    void destroy() noexcept {
#if defined(FV1_HAVE_SPEEXDSP)
        if (state_) speex_resampler_destroy(state_);
        state_ = nullptr;
#endif
    }
    double in_rate_{};
    double out_rate_{};
    bool bypass_{};
    bool use_speex_{};
    LinearStereoResampler linear_;
#if defined(FV1_HAVE_SPEEXDSP)
    SpeexResamplerState* state_{};
#endif
};

} // namespace

class Runtime::Impl {
public:
    RuntimeConfig cfg{};
    fv1_engine* chip{};
    StereoResampler host_to_fv1;
    StereoResampler fv1_to_host;
    std::vector<StereoFrame> input_scratch;
    std::vector<StereoFrame> fv1_input;
    std::vector<StereoFrame> fv1_output;
    std::vector<StereoFrame> host_resampled;
    FrameRing output_ring;
    std::atomic<std::uint64_t> host_input_frames{0};
    std::atomic<std::uint64_t> fv1_frames{0};
    std::atomic<std::uint64_t> host_output_frames{0};
    std::atomic<std::uint64_t> underrun_frames{0};
    std::atomic<std::uint64_t> overrun_frames{0};

    ~Impl() { if (chip) fv1_destroy(chip); }
};

Runtime::Runtime() : impl_(std::make_unique<Impl>()) {}
Runtime::~Runtime() = default;
Runtime::Runtime(Runtime&&) noexcept = default;
Runtime& Runtime::operator=(Runtime&&) noexcept = default;

bool Runtime::prepare(const RuntimeConfig& config) {
    if (!(config.host_sample_rate > 1000.0) || !(config.fv1_sample_rate > 1000.0) ||
        config.max_host_block_frames == 0) return false;

    impl_->cfg = config;
    if (impl_->chip) { fv1_destroy(impl_->chip); impl_->chip = nullptr; }
    const fv1_config core_cfg{config.fv1_sample_rate, config.delay_model};
    impl_->chip = fv1_create(&core_cfg);
    if (!impl_->chip) return false;

    if (!impl_->host_to_fv1.init(config.host_sample_rate, config.fv1_sample_rate,
                                 config.resampler_quality)) return false;
    if (!impl_->fv1_to_host.init(config.fv1_sample_rate, config.host_sample_rate,
                                 config.resampler_quality)) return false;

    const double up_ratio = std::max(1.0, config.fv1_sample_rate / config.host_sample_rate);
    const double down_ratio = std::max(1.0, config.host_sample_rate / config.fv1_sample_rate);
    const std::size_t fv1_capacity = static_cast<std::size_t>(
        std::ceil(static_cast<double>(config.max_host_block_frames) * up_ratio)) + 256;
    const std::size_t host_capacity = static_cast<std::size_t>(
        std::ceil(static_cast<double>(fv1_capacity) * down_ratio)) + 256;

    impl_->input_scratch.assign(config.max_host_block_frames, {});
    impl_->fv1_input.assign(fv1_capacity, {});
    impl_->fv1_output.assign(fv1_capacity, {});
    impl_->host_resampled.assign(host_capacity, {});
    impl_->output_ring.prepare(std::max<std::size_t>(host_capacity * 4, config.max_host_block_frames * 8));
    clear_stats();
    return true;
}

void Runtime::reset(bool clear_delay_ram) {
    if (!impl_->chip) return;
    fv1_reset(impl_->chip, clear_delay_ram ? 1 : 0);
    impl_->host_to_fv1.reset();
    impl_->fv1_to_host.reset();
    impl_->output_ring.clear();
    clear_stats();
}

fv1_engine* Runtime::engine() noexcept { return impl_->chip; }
const fv1_engine* Runtime::engine() const noexcept { return impl_->chip; }

bool Runtime::load_program_bytes(const std::uint8_t* bytes, std::size_t size) {
    return impl_->chip && fv1_load_bytes(impl_->chip, bytes, size) == FV1_OK;
}
void Runtime::set_pots(float a, float b, float c) { if (impl_->chip) fv1_set_pots(impl_->chip, a, b, c); }

bool Runtime::process_block(const StereoFrame* input, StereoFrame* output,
                            std::size_t host_frames) noexcept {
    if (!impl_->chip || !output || host_frames == 0 ||
        host_frames > impl_->cfg.max_host_block_frames) return false;

    const StereoFrame* src = input;
    if (!src) {
        std::fill_n(impl_->input_scratch.data(), host_frames, StereoFrame{});
        src = impl_->input_scratch.data();
    }

    std::size_t consumed = 0;
    const std::size_t fv_frames = impl_->host_to_fv1.process(
        src, host_frames, impl_->fv1_input.data(), impl_->fv1_input.size(), &consumed);
    if (consumed != host_frames) return false;

    for (std::size_t i = 0; i < fv_frames; ++i) {
        float l = 0.0f, r = 0.0f;
        if (fv1_process_sample(impl_->chip, impl_->fv1_input[i].left,
                               impl_->fv1_input[i].right, &l, &r) != FV1_OK) return false;
        impl_->fv1_output[i] = {l, r};
    }

    std::size_t fv_consumed = 0;
    const std::size_t host_produced = impl_->fv1_to_host.process(
        impl_->fv1_output.data(), fv_frames,
        impl_->host_resampled.data(), impl_->host_resampled.size(), &fv_consumed);
    if (fv_consumed != fv_frames && fv_frames != 0) return false;

    const std::size_t pushed = impl_->output_ring.push(impl_->host_resampled.data(), host_produced);
    if (pushed < host_produced) impl_->overrun_frames.fetch_add(host_produced - pushed, std::memory_order_relaxed);

    const std::size_t popped = impl_->output_ring.pop(output, host_frames);
    if (popped < host_frames) {
        std::fill(output + static_cast<std::ptrdiff_t>(popped),
                  output + static_cast<std::ptrdiff_t>(host_frames), StereoFrame{});
        impl_->underrun_frames.fetch_add(host_frames - popped, std::memory_order_relaxed);
    }

    impl_->host_input_frames.fetch_add(host_frames, std::memory_order_relaxed);
    impl_->fv1_frames.fetch_add(fv_frames, std::memory_order_relaxed);
    impl_->host_output_frames.fetch_add(host_frames, std::memory_order_relaxed);
    return true;
}

RuntimeConfig Runtime::config() const noexcept { return impl_->cfg; }
RuntimeStats Runtime::stats() const noexcept {
    return {
        impl_->host_input_frames.load(std::memory_order_relaxed),
        impl_->fv1_frames.load(std::memory_order_relaxed),
        impl_->host_output_frames.load(std::memory_order_relaxed),
        impl_->underrun_frames.load(std::memory_order_relaxed),
        impl_->overrun_frames.load(std::memory_order_relaxed)
    };
}
void Runtime::clear_stats() noexcept {
    impl_->host_input_frames.store(0, std::memory_order_relaxed);
    impl_->fv1_frames.store(0, std::memory_order_relaxed);
    impl_->host_output_frames.store(0, std::memory_order_relaxed);
    impl_->underrun_frames.store(0, std::memory_order_relaxed);
    impl_->overrun_frames.store(0, std::memory_order_relaxed);
}
bool Runtime::using_speexdsp() const noexcept {
    return impl_->host_to_fv1.using_speex() || impl_->fv1_to_host.using_speex();
}

} // namespace fv1
