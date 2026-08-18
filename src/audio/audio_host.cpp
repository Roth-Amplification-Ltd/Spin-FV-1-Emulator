#include <fv1/audio_host.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <vector>

#if defined(FV1_HAVE_MINIAUDIO)
#include <miniaudio.h>
#endif

namespace fv1 {

#if defined(FV1_HAVE_MINIAUDIO)
namespace {

ma_result init_platform_audio_context(ma_context* context) {
#if defined(_WIN32)
    const ma_backend backends[] = {
        ma_backend_wasapi
    };
    return ma_context_init(
        backends,
        static_cast<ma_uint32>(sizeof(backends) / sizeof(backends[0])),
        nullptr,
        context);
#else
    return ma_context_init(nullptr, 0, nullptr, context);
#endif
}

} // namespace

class AudioHost::Impl {
public:
    ma_context context{};
    ma_device device{};
    bool context_ready{};
    bool device_ready{};
    bool started{};
    AudioHostConfig cfg{};
    AudioSource* source{};
    Runtime* runtime{};
    AnalyzerWorker* analyzer{};
    AnalyzerWorker* raw_analyzer{};
    std::atomic<AudioRecorder*> recorder{nullptr};
    std::vector<StereoFrame> source_buffer;
    std::atomic<std::uint64_t> callbacks{0};
    std::atomic<std::uint64_t> source_frames{0};
    std::atomic<double> callback_cpu{0.0};
    std::atomic<bool> finished{false};
    std::atomic<bool> dsp_enabled{true};

    ~Impl() { close(); }

    void close() noexcept {
        if (device_ready) {
            if (started) ma_device_stop(&device);
            ma_device_uninit(&device);
        }
        device_ready = false;
        started = false;
        if (context_ready) ma_context_uninit(&context);
        context_ready = false;
        source = nullptr;
        runtime = nullptr;
        analyzer = nullptr;
        raw_analyzer = nullptr;
        recorder.store(nullptr, std::memory_order_release);
        source_buffer.clear();
    }

    static void callback(ma_device* dev, void* output, const void* input, ma_uint32 frame_count) {
        auto* self = static_cast<Impl*>(dev->pUserData);
        if (!self || !output || !self->source || !self->runtime) return;
        auto* out = static_cast<StereoFrame*>(output);
        const auto* in = static_cast<const StereoFrame*>(input);
        const std::size_t frames = frame_count;
        if (frames > self->source_buffer.size()) {
            std::fill_n(out, frames, StereoFrame{});
            return;
        }

        const std::uint64_t already = self->source_frames.load(std::memory_order_relaxed);
        std::size_t process_frames = frames;
        if (self->cfg.stop_after_frames != 0) {
            if (already >= self->cfg.stop_after_frames) {
                std::fill_n(out, frames, StereoFrame{});
                self->finished.store(true, std::memory_order_release);
                return;
            }
            const std::uint64_t remaining = self->cfg.stop_after_frames - already;
            process_frames = std::min<std::size_t>(frames, static_cast<std::size_t>(remaining));
        }

        const auto begin = std::chrono::steady_clock::now();
        self->source->render(in, self->source_buffer.data(), process_frames);
        if (self->raw_analyzer) self->raw_analyzer->push(self->source_buffer.data(), process_frames);
        if (auto* recorder = self->recorder.load(std::memory_order_acquire))
            recorder->push_raw(self->source_buffer.data(), process_frames);
        if (self->dsp_enabled.load(std::memory_order_relaxed)) {
            const bool ok = self->runtime->process_block(self->source_buffer.data(), out, process_frames);
            if (!ok) std::fill_n(out, process_frames, StereoFrame{});
        } else {
            std::copy_n(self->source_buffer.data(), process_frames, out);
        }
        if (process_frames < frames)
            std::fill(out + static_cast<std::ptrdiff_t>(process_frames),
                      out + static_cast<std::ptrdiff_t>(frames), StereoFrame{});
        if (self->analyzer) self->analyzer->push(out, process_frames);
        if (auto* recorder = self->recorder.load(std::memory_order_acquire))
            recorder->push_processed(out, process_frames);
        const auto end = std::chrono::steady_clock::now();

        const double elapsed = std::chrono::duration<double>(end - begin).count();
        const double available = static_cast<double>(process_frames) /
                                 static_cast<double>(self->cfg.host_sample_rate);
        const double instant = available > 0.0 ? elapsed / available : 0.0;
        const double previous = self->callback_cpu.load(std::memory_order_relaxed);
        self->callback_cpu.store(previous * 0.95 + instant * 0.05, std::memory_order_relaxed);
        self->callbacks.fetch_add(1, std::memory_order_relaxed);
        const std::uint64_t total = self->source_frames.fetch_add(process_frames, std::memory_order_relaxed) + process_frames;
        if (self->cfg.stop_after_frames != 0 && total >= self->cfg.stop_after_frames)
            self->finished.store(true, std::memory_order_release);
    }
};
#else
class AudioHost::Impl {
public:
    std::atomic<bool> dsp_enabled{true};
    std::atomic<AudioRecorder*> recorder{nullptr};
};
#endif

AudioHost::AudioHost() : impl_(std::make_unique<Impl>()) {}
AudioHost::~AudioHost() = default;

bool AudioHost::available() noexcept {
#if defined(FV1_HAVE_MINIAUDIO)
    return true;
#else
    return false;
#endif
}

std::vector<AudioDeviceInfo> AudioHost::enumerate(std::string* error) {
    std::vector<AudioDeviceInfo> out;
#if defined(FV1_HAVE_MINIAUDIO)
    ma_context ctx{};
    const ma_result init = init_platform_audio_context(&ctx);
    if (init != MA_SUCCESS) {
        if (error) *error = std::string("miniaudio context init failed: ") + ma_result_description(init);
        return out;
    }
    ma_device_info* playback = nullptr;
    ma_uint32 playback_count = 0;
    ma_device_info* capture = nullptr;
    ma_uint32 capture_count = 0;
    const ma_result rc = ma_context_get_devices(&ctx, &playback, &playback_count, &capture, &capture_count);
    if (rc != MA_SUCCESS) {
        if (error) *error = std::string("device enumeration failed: ") + ma_result_description(rc);
        ma_context_uninit(&ctx);
        return out;
    }
    out.reserve(static_cast<std::size_t>(playback_count + capture_count));
    for (ma_uint32 i = 0; i < playback_count; ++i)
        out.push_back({i, AudioDeviceDirection::Playback, playback[i].name, playback[i].isDefault == MA_TRUE});
    for (ma_uint32 i = 0; i < capture_count; ++i)
        out.push_back({i, AudioDeviceDirection::Capture, capture[i].name, capture[i].isDefault == MA_TRUE});
    ma_context_uninit(&ctx);
#else
    if (error) *error = "this build does not include miniaudio; install/vendor miniaudio.h and rebuild";
#endif
    return out;
}

bool AudioHost::open(const AudioHostConfig& config,
                     AudioSource& source,
                     Runtime& runtime,
                     AnalyzerWorker* analyzer,
                     std::string* error) {
    return open(config, source, runtime, analyzer, nullptr, error);
}

bool AudioHost::open(const AudioHostConfig& config,
                     AudioSource& source,
                     Runtime& runtime,
                     AnalyzerWorker* analyzer,
                     AnalyzerWorker* raw_analyzer,
                     std::string* error) {
#if defined(FV1_HAVE_MINIAUDIO)
    impl_->close();
    if (config.host_sample_rate < 8000 || config.period_frames == 0) {
        if (error) *error = "invalid audio-host sample rate or period size";
        return false;
    }
    const ma_result ctx_rc = init_platform_audio_context(&impl_->context);
    if (ctx_rc != MA_SUCCESS) {
        if (error) *error = std::string("miniaudio context init failed: ") + ma_result_description(ctx_rc);
        return false;
    }
    impl_->context_ready = true;

    ma_device_info* playback = nullptr;
    ma_uint32 playback_count = 0;
    ma_device_info* capture = nullptr;
    ma_uint32 capture_count = 0;
    const ma_result enum_rc = ma_context_get_devices(&impl_->context,
        &playback, &playback_count, &capture, &capture_count);
    if (enum_rc != MA_SUCCESS) {
        if (error) *error = std::string("device enumeration failed: ") + ma_result_description(enum_rc);
        impl_->close();
        return false;
    }

    if (config.playback_device >= static_cast<int>(playback_count) ||
        config.capture_device >= static_cast<int>(capture_count)) {
        if (error) *error = "selected audio device index is out of range";
        impl_->close();
        return false;
    }

    ma_device_config dc = ma_device_config_init(config.needs_capture ? ma_device_type_duplex : ma_device_type_playback);
    dc.playback.format = ma_format_f32;
    dc.playback.channels = 2;
    dc.sampleRate = config.host_sample_rate;
    dc.periodSizeInFrames = config.period_frames;
    dc.dataCallback = &Impl::callback;
    dc.pUserData = impl_.get();
    if (config.playback_device >= 0)
        dc.playback.pDeviceID = &playback[static_cast<ma_uint32>(config.playback_device)].id;
    if (config.needs_capture) {
        dc.capture.format = ma_format_f32;
        dc.capture.channels = 2;
        if (config.capture_device >= 0)
            dc.capture.pDeviceID = &capture[static_cast<ma_uint32>(config.capture_device)].id;
    }

    impl_->cfg = config;
    impl_->source = &source;
    impl_->runtime = &runtime;
    impl_->analyzer = analyzer;
    impl_->raw_analyzer = raw_analyzer;
    impl_->source_buffer.assign(std::max<std::size_t>(config.period_frames * 4u,
                                                     runtime.config().max_host_block_frames), {});
    if (!source.prepare(config.host_sample_rate, runtime.config().max_host_block_frames)) {
        if (error) *error = "audio source could not be prepared for the host sample rate";
        impl_->close();
        return false;
    }

    const ma_result dev_rc = ma_device_init(&impl_->context, &dc, &impl_->device);
    if (dev_rc != MA_SUCCESS) {
        if (error) *error = std::string("audio device init failed: ") + ma_result_description(dev_rc);
        impl_->close();
        return false;
    }
    impl_->device_ready = true;
    impl_->callbacks.store(0, std::memory_order_relaxed);
    impl_->source_frames.store(0, std::memory_order_relaxed);
    impl_->callback_cpu.store(0.0, std::memory_order_relaxed);
    impl_->finished.store(false, std::memory_order_relaxed);
    impl_->dsp_enabled.store(true, std::memory_order_relaxed);
    return true;
#else
    (void)config; (void)source; (void)runtime; (void)analyzer; (void)raw_analyzer;
    if (error) *error = "this build does not include miniaudio; run the platform development bootstrap and rebuild";
    return false;
#endif
}

bool AudioHost::start(std::string* error) {
#if defined(FV1_HAVE_MINIAUDIO)
    if (!impl_->device_ready) {
        if (error) *error = "audio device is not open";
        return false;
    }
    if (impl_->started) return true;
    const ma_result rc = ma_device_start(&impl_->device);
    if (rc != MA_SUCCESS) {
        if (error) *error = std::string("audio device start failed: ") + ma_result_description(rc);
        return false;
    }
    impl_->started = true;
    return true;
#else
    if (error) *error = "miniaudio unavailable";
    return false;
#endif
}

void AudioHost::stop() noexcept {
#if defined(FV1_HAVE_MINIAUDIO)
    if (impl_->started && impl_->device_ready) ma_device_stop(&impl_->device);
    impl_->started = false;
#endif
}
void AudioHost::close() noexcept {
#if defined(FV1_HAVE_MINIAUDIO)
    impl_->close();
#endif
}
bool AudioHost::is_open() const noexcept {
#if defined(FV1_HAVE_MINIAUDIO)
    return impl_->device_ready;
#else
    return false;
#endif
}
bool AudioHost::is_started() const noexcept {
#if defined(FV1_HAVE_MINIAUDIO)
    return impl_->started;
#else
    return false;
#endif
}
bool AudioHost::is_finished() const noexcept {
#if defined(FV1_HAVE_MINIAUDIO)
    return impl_->finished.load(std::memory_order_acquire);
#else
    return false;
#endif
}
AudioHostStats AudioHost::stats() const noexcept {
#if defined(FV1_HAVE_MINIAUDIO)
    return {impl_->callback_cpu.load(std::memory_order_relaxed),
            impl_->callbacks.load(std::memory_order_relaxed),
            impl_->source_frames.load(std::memory_order_relaxed)};
#else
    return {};
#endif
}

void AudioHost::set_dsp_enabled(bool enabled) noexcept {
    impl_->dsp_enabled.store(enabled, std::memory_order_release);
}

bool AudioHost::dsp_enabled() const noexcept {
    return impl_->dsp_enabled.load(std::memory_order_acquire);
}

void AudioHost::set_recorder(AudioRecorder* recorder) noexcept {
    impl_->recorder.store(recorder, std::memory_order_release);
}

AudioRecorder* AudioHost::recorder() const noexcept {
    return impl_->recorder.load(std::memory_order_acquire);
}

} // namespace fv1
