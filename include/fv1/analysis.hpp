#pragma once

#include <fv1/runtime.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace fv1 {

struct AnalysisSnapshot {
    double sample_rate{};
    std::uint64_t sequence{};
    float peak_left{};
    float peak_right{};
    float rms_left{};
    float rms_right{};
    float correlation{};            // -1..+1
    float dominant_frequency_hz{};
    float dominant_level_db{};
    std::vector<float> spectrum_db; // N/2+1 mono magnitude bins.
};

/* Realtime-safe producer / background-worker analyzer. push() never allocates,
   never locks, and drops frames rather than blocking the audio callback. */
class AnalyzerWorker {
public:
    AnalyzerWorker();
    ~AnalyzerWorker();
    AnalyzerWorker(const AnalyzerWorker&) = delete;
    AnalyzerWorker& operator=(const AnalyzerWorker&) = delete;

    bool prepare(double sample_rate,
                 std::size_t fft_size = 1024,
                 std::size_t queue_frames = 16384);
    void start();
    void stop();

    void push(const StereoFrame* frames, std::size_t count) noexcept;
    AnalysisSnapshot latest() const;
    std::uint64_t dropped_frames() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fv1
