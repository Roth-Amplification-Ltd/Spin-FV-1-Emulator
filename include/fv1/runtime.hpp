#pragma once

#include <fv1/fv1.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace fv1 {

/* Stereo sample used by the runtime, source router and analyzer queues.
   Interleaved stereo keeps the realtime/device boundary simple and matches
   the f32 data format requested from miniaudio. */
struct StereoFrame {
    float left{};
    float right{};
};

struct RuntimeConfig {
    double host_sample_rate{48000.0};
    double fv1_sample_rate{32768.0};
    std::size_t max_host_block_frames{1024};
    int resampler_quality{7};              // SpeexDSP 0..10 when available.
    fv1_delay_model delay_model{FV1_DELAY_REFERENCE_16};
};

struct RuntimeStats {
    std::uint64_t host_input_frames{};
    std::uint64_t fv1_frames{};
    std::uint64_t host_output_frames{};
    std::uint64_t output_underrun_frames{};
    std::uint64_t output_ring_overrun_frames{};
};

/* Runtime owns the virtual chip and the two clock-domain crossings:

       host Fs -> virtual FV-1 Fs -> host Fs

   prepare() performs every allocation required by process_block().  The
   process path therefore performs no heap allocation and takes no locks. */
class Runtime {
public:
    Runtime();
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) noexcept;
    Runtime& operator=(Runtime&&) noexcept;

    bool prepare(const RuntimeConfig& config);
    void reset(bool clear_delay_ram = true);

    fv1_engine* engine() noexcept;
    const fv1_engine* engine() const noexcept;

    bool load_program_bytes(const std::uint8_t* bytes, std::size_t size);
    void set_pots(float pot0, float pot1, float pot2);

    /* Process exactly host_frames interleaved stereo frames.  input may be
       nullptr (silence). output must contain room for host_frames frames. */
    bool process_block(const StereoFrame* input,
                       StereoFrame* output,
                       std::size_t host_frames) noexcept;

    RuntimeConfig config() const noexcept;
    RuntimeStats stats() const noexcept;
    void clear_stats() noexcept;
    bool using_speexdsp() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fv1
