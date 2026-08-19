#include <fv1/audio_host.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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

std::string persistent_device_id(
    const ma_context& context,
    const ma_device_id& id
) {
#if defined(_WIN32)
    if (context.backend == ma_backend_wasapi) {
        const wchar_t* wide = id.wasapi;
        if (!wide || wide[0] == L'\0') return {};

        const int required = WideCharToMultiByte(
            CP_UTF8,
            0,
            wide,
            -1,
            nullptr,
            0,
            nullptr,
            nullptr);
        if (required <= 1) return {};

        std::string utf8(static_cast<std::size_t>(required), '\0');
        const int written = WideCharToMultiByte(
            CP_UTF8,
            0,
            wide,
            -1,
            utf8.data(),
            required,
            nullptr,
            nullptr);
        if (written <= 1) return {};

        utf8.resize(static_cast<std::size_t>(written - 1));
        return std::string("wasapi:") + utf8;
    }
#else
    (void)context;
    (void)id;
#endif
    return {};
}

void add_native_rate(
    std::vector<std::uint32_t>& rates,
    std::uint32_t rate
) {
    if (rate == 0) return;
    if (std::find(rates.begin(), rates.end(), rate) == rates.end())
        rates.push_back(rate);
}

AudioDeviceInfo make_device_info(
    ma_context& context,
    ma_device_type type,
    std::uint32_t index,
    const ma_device_info& basic
) {
    AudioDeviceInfo out;
    out.index = index;
    out.direction =
        type == ma_device_type_playback
            ? AudioDeviceDirection::Playback
            : AudioDeviceDirection::Capture;
    out.name = basic.name;
    out.is_default = basic.isDefault == MA_TRUE;
    out.persistent_id = persistent_device_id(context, basic.id);

    ma_device_info detail{};
    if (ma_context_get_device_info(
            &context,
            type,
            &basic.id,
            &detail) == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < detail.nativeDataFormatCount; ++i) {
            add_native_rate(
                out.native_sample_rates,
                detail.nativeDataFormats[i].sampleRate);
            out.max_channels = std::max(
                out.max_channels,
                static_cast<std::uint32_t>(
                    detail.nativeDataFormats[i].channels));
        }
        std::sort(
            out.native_sample_rates.begin(),
            out.native_sample_rates.end());
    }

    return out;
}

bool decode_persistent_device_id(
    const ma_context& context,
    const std::string& persistent_id,
    ma_device_id& out
) {
#if defined(_WIN32)
    if (context.backend != ma_backend_wasapi) return false;

    constexpr std::string_view prefix{"wasapi:"};
    if (persistent_id.size() <= prefix.size() ||
        persistent_id.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }

    const std::string payload = persistent_id.substr(prefix.size());

    const int required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        payload.c_str(),
        -1,
        nullptr,
        0);
    if (required <= 1 ||
        required > static_cast<int>(
            sizeof(out.wasapi) / sizeof(out.wasapi[0]))) {
        return false;
    }

    std::memset(&out, 0, sizeof(out));
    const int written = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        payload.c_str(),
        -1,
        out.wasapi,
        static_cast<int>(
            sizeof(out.wasapi) / sizeof(out.wasapi[0])));

    return written == required;
#else
    (void)context;
    (void)persistent_id;
    (void)out;
    return false;
#endif
}

void update_atomic_min(
    std::atomic<std::uint32_t>& value,
    std::uint32_t candidate
) noexcept {
    std::uint32_t current = value.load(std::memory_order_relaxed);
    while (candidate < current &&
           !value.compare_exchange_weak(
               current,
               candidate,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
    }
}

void update_atomic_max(
    std::atomic<std::uint32_t>& value,
    std::uint32_t candidate
) noexcept {
    std::uint32_t current = value.load(std::memory_order_relaxed);
    while (candidate > current &&
           !value.compare_exchange_weak(
               current,
               candidate,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
    }
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

    std::atomic<std::uint32_t> min_callback_frames{
        std::numeric_limits<std::uint32_t>::max()};
    std::atomic<std::uint32_t> max_callback_frames{0};

    std::atomic<std::uint64_t> device_started_events{0};
    std::atomic<std::uint64_t> device_stopped_events{0};
    std::atomic<std::uint64_t> device_reroute_events{0};
    std::atomic<std::uint64_t> device_interruption_events{0};
    std::atomic<bool> device_running{false};
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> unexpected_device_stop{false};

    std::uint32_t callback_sample_rate{};
    std::uint32_t playback_native_sample_rate{};
    std::uint32_t capture_native_sample_rate{};
    std::uint32_t playback_period_frames{};
    std::uint32_t capture_period_frames{};

    ~Impl() { close(); }

    void reset_diagnostics() noexcept {
        callbacks.store(0, std::memory_order_relaxed);
        source_frames.store(0, std::memory_order_relaxed);
        callback_cpu.store(0.0, std::memory_order_relaxed);
        finished.store(false, std::memory_order_relaxed);

        min_callback_frames.store(
            std::numeric_limits<std::uint32_t>::max(),
            std::memory_order_relaxed);
        max_callback_frames.store(0, std::memory_order_relaxed);

        device_started_events.store(0, std::memory_order_relaxed);
        device_stopped_events.store(0, std::memory_order_relaxed);
        device_reroute_events.store(0, std::memory_order_relaxed);
        device_interruption_events.store(0, std::memory_order_relaxed);
        device_running.store(false, std::memory_order_relaxed);
        stop_requested.store(false, std::memory_order_relaxed);
        unexpected_device_stop.store(false, std::memory_order_relaxed);

        callback_sample_rate = 0;
        playback_native_sample_rate = 0;
        capture_native_sample_rate = 0;
        playback_period_frames = 0;
        capture_period_frames = 0;
    }

    void close() noexcept {
        stop_requested.store(true, std::memory_order_release);
        if (device_ready) {
            if (started) ma_device_stop(&device);
            ma_device_uninit(&device);
        }
        device_ready = false;
        started = false;
        device_running.store(false, std::memory_order_release);
        if (context_ready) ma_context_uninit(&context);
        context_ready = false;
        source = nullptr;
        runtime = nullptr;
        analyzer = nullptr;
        raw_analyzer = nullptr;
        recorder.store(nullptr, std::memory_order_release);
        source_buffer.clear();
    }

    static void notification(
        const ma_device_notification* notification
    ) noexcept {
        if (!notification || !notification->pDevice) return;
        auto* self =
            static_cast<Impl*>(notification->pDevice->pUserData);
        if (!self) return;

        switch (notification->type) {
        case ma_device_notification_type_started:
            self->device_started_events.fetch_add(
                1,
                std::memory_order_relaxed);
            self->device_running.store(true, std::memory_order_release);
            break;

        case ma_device_notification_type_stopped:
            self->device_stopped_events.fetch_add(
                1,
                std::memory_order_relaxed);
            self->device_running.store(false, std::memory_order_release);
            if (!self->stop_requested.load(std::memory_order_acquire))
                self->unexpected_device_stop.store(
                    true,
                    std::memory_order_release);
            break;

        case ma_device_notification_type_rerouted:
            self->device_reroute_events.fetch_add(
                1,
                std::memory_order_relaxed);
            break;

        case ma_device_notification_type_interruption_began:
        case ma_device_notification_type_interruption_ended:
            self->device_interruption_events.fetch_add(
                1,
                std::memory_order_relaxed);
            break;

        case ma_device_notification_type_unlocked:
            break;
        }
    }

    static void callback(ma_device* dev, void* output, const void* input, ma_uint32 frame_count) {
        auto* self = static_cast<Impl*>(dev->pUserData);
        if (!self || !output || !self->source || !self->runtime) return;
        auto* out = static_cast<StereoFrame*>(output);
        const auto* in = static_cast<const StereoFrame*>(input);
        const std::size_t frames = frame_count;
        update_atomic_min(
            self->min_callback_frames,
            static_cast<std::uint32_t>(frame_count));
        update_atomic_max(
            self->max_callback_frames,
            static_cast<std::uint32_t>(frame_count));

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

std::string AudioHost::backend_name() {
#if !defined(FV1_HAVE_MINIAUDIO)
    return "Unavailable (miniaudio not compiled)";
#elif defined(_WIN32)
    return "WASAPI via miniaudio";
#elif defined(__APPLE__)
    return "Core Audio via miniaudio";
#else
    return "miniaudio / system audio";
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
        out.push_back(make_device_info(
            ctx,
            ma_device_type_playback,
            static_cast<std::uint32_t>(i),
            playback[i]));
    for (ma_uint32 i = 0; i < capture_count; ++i)
        out.push_back(make_device_info(
            ctx,
            ma_device_type_capture,
            static_cast<std::uint32_t>(i),
            capture[i]));
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
    impl_->reset_diagnostics();
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

    int playback_index = config.playback_device;
    int capture_index = config.capture_device;

    ma_device_id playback_id_storage{};
    ma_device_id capture_id_storage{};
    bool playback_id_ready = false;
    bool capture_id_ready = false;

    /*
     * Windows endpoint IDs are persistent opaque strings. Do not re-enumerate
     * and try to rediscover an index for an already selected endpoint. Decode
     * the stored opaque WASAPI ID back into miniaudio's ma_device_id and pass
     * it directly to ma_device_init().
     */
    if (!config.playback_device_id.empty()) {
        if (!decode_persistent_device_id(
                impl_->context,
                config.playback_device_id,
                playback_id_storage)) {
            if (error) *error =
                "selected playback endpoint ID is invalid for the active audio backend";
            impl_->close();
            return false;
        }
        playback_id_ready = true;
        playback_index = -1;
    }

    if (config.needs_capture && !config.capture_device_id.empty()) {
        if (!decode_persistent_device_id(
                impl_->context,
                config.capture_device_id,
                capture_id_storage)) {
            if (error) *error =
                "selected capture endpoint ID is invalid for the active audio backend";
            impl_->close();
            return false;
        }
        capture_id_ready = true;
        capture_index = -1;
    }

    if ((!playback_id_ready &&
         playback_index >= static_cast<int>(playback_count)) ||
        (config.needs_capture &&
         !capture_id_ready &&
         capture_index >= static_cast<int>(capture_count))) {
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
    dc.notificationCallback = &Impl::notification;
    dc.pUserData = impl_.get();
#if defined(_WIN32)
    dc.performanceProfile = ma_performance_profile_low_latency;
    dc.wasapi.usage = ma_wasapi_usage_pro_audio;
#endif
    if (playback_id_ready) {
        dc.playback.pDeviceID = &playback_id_storage;
    } else if (playback_index >= 0) {
        dc.playback.pDeviceID =
            &playback[static_cast<ma_uint32>(playback_index)].id;
    }
    if (config.needs_capture) {
        dc.capture.format = ma_format_f32;
        dc.capture.channels = 2;
        if (capture_id_ready) {
            dc.capture.pDeviceID = &capture_id_storage;
        } else if (capture_index >= 0) {
            dc.capture.pDeviceID =
                &capture[static_cast<ma_uint32>(capture_index)].id;
        }
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
    impl_->callback_sample_rate = impl_->device.sampleRate;
    impl_->playback_native_sample_rate =
        impl_->device.playback.internalSampleRate;
    impl_->playback_period_frames =
        impl_->device.playback.internalPeriodSizeInFrames;
    if (config.needs_capture) {
        impl_->capture_native_sample_rate =
            impl_->device.capture.internalSampleRate;
        impl_->capture_period_frames =
            impl_->device.capture.internalPeriodSizeInFrames;
    }
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
    impl_->stop_requested.store(false, std::memory_order_release);
    impl_->unexpected_device_stop.store(false, std::memory_order_release);
    const ma_result rc = ma_device_start(&impl_->device);
    if (rc != MA_SUCCESS) {
        if (error) *error = std::string("audio device start failed: ") + ma_result_description(rc);
        return false;
    }
    impl_->started = true;
    impl_->device_running.store(true, std::memory_order_release);
    return true;
#else
    if (error) *error = "miniaudio unavailable";
    return false;
#endif
}

void AudioHost::stop() noexcept {
#if defined(FV1_HAVE_MINIAUDIO)
    impl_->stop_requested.store(true, std::memory_order_release);
    if (impl_->started && impl_->device_ready) ma_device_stop(&impl_->device);
    impl_->started = false;
    impl_->device_running.store(false, std::memory_order_release);
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
    return impl_->device_running.load(std::memory_order_acquire);
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
    AudioHostStats out;
    out.callback_cpu_load =
        impl_->callback_cpu.load(std::memory_order_relaxed);
    out.callbacks =
        impl_->callbacks.load(std::memory_order_relaxed);
    out.source_frames =
        impl_->source_frames.load(std::memory_order_relaxed);

    out.callback_sample_rate = impl_->callback_sample_rate;
    out.playback_native_sample_rate =
        impl_->playback_native_sample_rate;
    out.capture_native_sample_rate =
        impl_->capture_native_sample_rate;
    out.playback_period_frames =
        impl_->playback_period_frames;
    out.capture_period_frames =
        impl_->capture_period_frames;

    const std::uint32_t min_frames =
        impl_->min_callback_frames.load(std::memory_order_relaxed);
    out.min_callback_frames =
        min_frames == std::numeric_limits<std::uint32_t>::max()
            ? 0
            : min_frames;
    out.max_callback_frames =
        impl_->max_callback_frames.load(std::memory_order_relaxed);

    out.device_started_events =
        impl_->device_started_events.load(std::memory_order_relaxed);
    out.device_stopped_events =
        impl_->device_stopped_events.load(std::memory_order_relaxed);
    out.device_reroute_events =
        impl_->device_reroute_events.load(std::memory_order_relaxed);
    out.device_interruption_events =
        impl_->device_interruption_events.load(std::memory_order_relaxed);
    out.device_running =
        impl_->device_running.load(std::memory_order_acquire);
    out.unexpected_device_stop =
        impl_->unexpected_device_stop.load(std::memory_order_acquire);
    return out;
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
