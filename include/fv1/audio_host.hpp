#pragma once

#include <fv1/analysis.hpp>
#include <fv1/audio_source.hpp>
#include <fv1/audio_recorder.hpp>
#include <fv1/runtime.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fv1 {

enum class AudioDeviceDirection { Playback, Capture };

struct AudioDeviceInfo {
    std::uint32_t index{};
    AudioDeviceDirection direction{AudioDeviceDirection::Playback};
    std::string name;
    bool is_default{};

    // Stable platform endpoint token when the backend exposes one. On Windows
    // this is the WASAPI endpoint ID and survives enumeration-order changes.
    std::string persistent_id;

    // Detailed native capabilities are best-effort. Empty/zero means the
    // backend did not report detailed information.
    std::vector<std::uint32_t> native_sample_rates;
    std::uint32_t max_channels{};
};

struct AudioHostConfig {
    std::uint32_t host_sample_rate{48000};
    std::uint32_t period_frames{256};
    int playback_device{-1}; // -1 = OS default / legacy index fallback
    int capture_device{-1};  // -1 = OS default / legacy index fallback

    // Preferred stable endpoint selectors. If non-empty these take precedence
    // over the legacy enumeration indices above.
    std::string playback_device_id;
    std::string capture_device_id;

    bool needs_capture{true};
    // 0 = unlimited.  For timed/reproducible sessions the callback processes
    // exactly this many host frames, zero-fills the remainder of the final
    // device period, then reports finished to the controlling thread.
    std::uint64_t stop_after_frames{};
};

struct AudioHostStats {
    double callback_cpu_load{}; // callback wall time / available audio time
    std::uint64_t callbacks{};
    std::uint64_t source_frames{};

    // Host/device geometry actually observed after backend negotiation.
    std::uint32_t callback_sample_rate{};
    std::uint32_t playback_native_sample_rate{};
    std::uint32_t capture_native_sample_rate{};
    std::uint32_t playback_period_frames{};
    std::uint32_t capture_period_frames{};
    std::uint32_t min_callback_frames{};
    std::uint32_t max_callback_frames{};

    // Device notifications are collected atomically. No start/stop/reopen
    // operation is performed from the notification callback.
    std::uint64_t device_started_events{};
    std::uint64_t device_stopped_events{};
    std::uint64_t device_reroute_events{};
    std::uint64_t device_interruption_events{};
    bool device_running{};
    bool unexpected_device_stop{};
};

/* Desktop miniaudio host. The public class intentionally contains no
   miniaudio types so the application never depends directly on that API. */
class AudioHost {
public:
    AudioHost();
    ~AudioHost();
    AudioHost(const AudioHost&) = delete;
    AudioHost& operator=(const AudioHost&) = delete;

    static bool available() noexcept;
    static std::string backend_name();
    static std::vector<AudioDeviceInfo> enumerate(std::string* error = nullptr);

    bool open(const AudioHostConfig& config,
              AudioSource& source,
              Runtime& runtime,
              AnalyzerWorker* analyzer,
              std::string* error = nullptr);

    /* Extended testbench form: output_analyzer receives the post-DSP/output
       stream while raw_input_analyzer receives the source before the FV-1.
       Either analyzer may be null. The legacy overload above remains the
       normal embedding API and simply omits the raw tap. */
    bool open(const AudioHostConfig& config,
              AudioSource& source,
              Runtime& runtime,
              AnalyzerWorker* output_analyzer,
              AnalyzerWorker* raw_input_analyzer,
              std::string* error);
    bool start(std::string* error = nullptr);
    void stop() noexcept;
    void close() noexcept;
    bool is_open() const noexcept;
    bool is_started() const noexcept;
    bool is_finished() const noexcept;
    AudioHostStats stats() const noexcept;

    /* Realtime-safe processing bypass.  When disabled, the host copies the
       selected source directly to output and to the analyzer, allowing the GUI
       to monitor the raw signal without tearing down the audio device. */
    void set_dsp_enabled(bool enabled) noexcept;
    bool dsp_enabled() const noexcept;

    /* Attach/detach a realtime-safe recorder without reopening the device.
       The recorder object must outlive its attachment. Passing nullptr detaches
       it immediately; all actual disk I/O remains on the recorder worker. */
    void set_recorder(AudioRecorder* recorder) noexcept;
    AudioRecorder* recorder() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fv1
