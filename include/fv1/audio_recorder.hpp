#pragma once

#include <fv1/runtime.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace fv1 {

enum class AudioRecordMode {
    Processed,
    Raw,
    RawAndProcessed,
};

struct AudioRecorderStats {
    std::uint64_t raw_frames_written{};
    std::uint64_t processed_frames_written{};
    std::uint64_t raw_frames_dropped{};
    std::uint64_t processed_frames_dropped{};
};

/*
 * Realtime-safe stereo WAV recorder.
 *
 * The audio callback only pushes frames into fixed-capacity SPSC rings. A
 * background thread performs all filesystem I/O and finalizes the RIFF/WAVE
 * headers when recording stops. Files are written as stereo 32-bit IEEE-float
 * WAV at the host sample rate so no additional quantization is introduced.
 */
class AudioRecorder {
public:
    AudioRecorder();
    ~AudioRecorder();
    AudioRecorder(const AudioRecorder&) = delete;
    AudioRecorder& operator=(const AudioRecorder&) = delete;

    /* base_path is used directly for single-stream recording. For
       RawAndProcessed, "-raw" and "-processed" are appended before the file
       extension. The extension defaults to .wav when omitted. */
    bool prepare(const std::filesystem::path& base_path,
                 std::uint32_t sample_rate,
                 AudioRecordMode mode,
                 std::size_t queue_frames = 262144,
                 std::string* error = nullptr);

    bool start(std::string* error = nullptr);
    void stop() noexcept;

    bool prepared() const noexcept;
    bool recording() const noexcept;
    AudioRecordMode mode() const noexcept;

    /* These are the only methods intended to be called by the realtime audio
       callback. They never allocate, lock, wait, or perform file I/O. */
    void push_raw(const StereoFrame* frames, std::size_t count) noexcept;
    void push_processed(const StereoFrame* frames, std::size_t count) noexcept;

    AudioRecorderStats stats() const noexcept;
    std::filesystem::path raw_path() const;
    std::filesystem::path processed_path() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fv1
