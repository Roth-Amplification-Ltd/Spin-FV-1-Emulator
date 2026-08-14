#pragma once

#include <fv1/runtime.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace fv1 {

/* Every source renders host-rate stereo into the same runtime input buffer.
   live_input is the capture buffer supplied by the audio backend.  Sources
   that do not use capture simply ignore it. */
class AudioSource {
public:
    virtual ~AudioSource() = default;
    virtual bool prepare(double host_sample_rate, std::size_t max_block_frames) = 0;
    virtual void reset() noexcept = 0;
    virtual void render(const StereoFrame* live_input,
                        StereoFrame* destination,
                        std::size_t frames) noexcept = 0;
    virtual const char* name() const noexcept = 0;
};

class LiveInputSource final : public AudioSource {
public:
    bool prepare(double host_sample_rate, std::size_t max_block_frames) override;
    void reset() noexcept override;
    void render(const StereoFrame* live_input, StereoFrame* destination,
                std::size_t frames) noexcept override;
    const char* name() const noexcept override { return "live-input"; }
};

enum class TransportState { Stopped, Playing, Paused };

class FileLoopSource final : public AudioSource {
public:
    FileLoopSource();
    ~FileLoopSource();
    FileLoopSource(const FileLoopSource&) = delete;
    FileLoopSource& operator=(const FileLoopSource&) = delete;

    bool load(const std::filesystem::path& path, std::string* error = nullptr);
    bool prepare(double host_sample_rate, std::size_t max_block_frames) override;
    void reset() noexcept override;
    void render(const StereoFrame* live_input, StereoFrame* destination,
                std::size_t frames) noexcept override;
    const char* name() const noexcept override { return "file-loop"; }

    void play() noexcept;
    void pause() noexcept;
    void stop() noexcept;
    void set_looping(bool enabled) noexcept;
    bool looping() const noexcept;
    TransportState state() const noexcept;

    /* Loop points are expressed in source-file frames and use [begin, end).
       end == 0 means the end of the file. Invalid ranges are rejected. */
    bool set_loop_region(std::uint64_t begin_frame, std::uint64_t end_frame) noexcept;
    std::uint64_t total_frames() const noexcept;
    std::uint32_t file_sample_rate() const noexcept;
    double position_seconds() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

enum class TestSignalKind { Sine, Sweep, WhiteNoise, PinkNoise, Impulse };

struct TestSignalConfig {
    TestSignalKind kind{TestSignalKind::Sine};
    double frequency_hz{440.0};
    double sweep_end_hz{12000.0};
    double sweep_seconds{5.0};
    double amplitude{0.25};
    double impulse_period_seconds{1.0};
    std::uint32_t noise_seed{0x465631u};
};

class TestSignalSource final : public AudioSource {
public:
    explicit TestSignalSource(TestSignalConfig config = {});
    bool prepare(double host_sample_rate, std::size_t max_block_frames) override;
    void reset() noexcept override;
    void render(const StereoFrame* live_input, StereoFrame* destination,
                std::size_t frames) noexcept override;
    const char* name() const noexcept override { return "test-generator"; }
    void configure(const TestSignalConfig& config) noexcept;
    TestSignalConfig config() const noexcept;

private:
    TestSignalConfig config_{};
    double sample_rate_{48000.0};
    double phase_{};
    std::uint64_t sample_index_{};
    std::uint32_t rng_{0x465631u};
    double pink0_{}, pink1_{}, pink2_{};
};

} // namespace fv1
