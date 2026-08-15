#pragma once

#include <fv1/sdk.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace fv1::windows_frontend {

class RealtimeProcessor final {
public:
    RealtimeProcessor();
    ~RealtimeProcessor();

    RealtimeProcessor(const RealtimeProcessor&) = delete;
    RealtimeProcessor& operator=(const RealtimeProcessor&) = delete;

    [[nodiscard]] bool ready() const noexcept { return engine_ != nullptr; }
    [[nodiscard]] bool program_loaded() const noexcept { return program_loaded_; }

    fv1_sdk_result load_program(const std::array<std::uint8_t, FV1_SDK_PROGRAM_BYTES>& program);
    fv1_sdk_result set_pots(const std::array<float, 3>& pots);
    fv1_sdk_result reset(bool clear_delay_ram = true);
    fv1_sdk_result process_sample(float input_left, float input_right,
                                  float& output_left, float& output_right);
    fv1_sdk_result process_interleaved(const float* input_stereo, float* output_stereo,
                                       std::size_t frames);
    fv1_sdk_result snapshot(fv1_sdk_snapshot_v1& out) const;

private:
    fv1_sdk_engine* engine_{};
    bool program_loaded_{};
};

} // namespace fv1::windows_frontend
