#pragma once

#include <fv1/analysis.hpp>
#include <fv1/audio_source.hpp>
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
};

struct AudioHostConfig {
    std::uint32_t host_sample_rate{48000};
    std::uint32_t period_frames{256};
    int playback_device{-1}; // -1 = OS default
    int capture_device{-1};  // -1 = OS default
    bool needs_capture{true};
};

struct AudioHostStats {
    double callback_cpu_load{}; // callback wall time / available audio time
    std::uint64_t callbacks{};
    std::uint64_t source_frames{};
};

/* Linux-first miniaudio host.  The public class intentionally contains no
   miniaudio types so the rest of the application never depends on that API. */
class AudioHost {
public:
    AudioHost();
    ~AudioHost();
    AudioHost(const AudioHost&) = delete;
    AudioHost& operator=(const AudioHost&) = delete;

    static bool available() noexcept;
    static std::vector<AudioDeviceInfo> enumerate(std::string* error = nullptr);

    bool open(const AudioHostConfig& config,
              AudioSource& source,
              Runtime& runtime,
              AnalyzerWorker* analyzer,
              std::string* error = nullptr);
    bool start(std::string* error = nullptr);
    void stop() noexcept;
    void close() noexcept;
    bool is_open() const noexcept;
    bool is_started() const noexcept;
    AudioHostStats stats() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fv1
