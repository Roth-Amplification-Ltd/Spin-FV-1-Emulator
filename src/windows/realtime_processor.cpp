#include "realtime_processor.hpp"

#include <algorithm>

namespace fv1::windows_frontend {

RealtimeProcessor::RealtimeProcessor() {
    fv1_sdk_engine_config_v1 config{};
    fv1_sdk_engine_config_v1_init(&config);
    (void)fv1_sdk_engine_create_v1(&config, &engine_);
}

RealtimeProcessor::~RealtimeProcessor() {
    if (engine_) {
        fv1_sdk_engine_destroy(engine_);
        engine_ = nullptr;
    }
}

fv1_sdk_result RealtimeProcessor::load_program(
    const std::array<std::uint8_t, FV1_SDK_PROGRAM_BYTES>& program) {
    if (!engine_) return FV1_SDK_ERROR_BAD_STATE;
    const auto result = fv1_sdk_engine_load_program(engine_, program.data(), program.size());
    if (result == FV1_SDK_OK) program_loaded_ = true;
    return result;
}

fv1_sdk_result RealtimeProcessor::set_pots(const std::array<float, 3>& pots) {
    if (!engine_) return FV1_SDK_ERROR_BAD_STATE;
    return fv1_sdk_engine_set_pots(
        engine_,
        std::clamp(pots[0], 0.0F, 1.0F),
        std::clamp(pots[1], 0.0F, 1.0F),
        std::clamp(pots[2], 0.0F, 1.0F));
}

fv1_sdk_result RealtimeProcessor::reset(bool clear_delay_ram) {
    if (!engine_) return FV1_SDK_ERROR_BAD_STATE;
    return fv1_sdk_engine_reset(engine_, clear_delay_ram ? 1u : 0u);
}

fv1_sdk_result RealtimeProcessor::process_sample(float input_left, float input_right,
                                                 float& output_left, float& output_right) {
    if (!engine_ || !program_loaded_) return FV1_SDK_ERROR_BAD_STATE;
    return fv1_sdk_engine_process_sample_f32(
        engine_, input_left, input_right, &output_left, &output_right);
}

fv1_sdk_result RealtimeProcessor::process_interleaved(const float* input_stereo,
                                                      float* output_stereo,
                                                      std::size_t frames) {
    if (!engine_ || !program_loaded_) return FV1_SDK_ERROR_BAD_STATE;
    return fv1_sdk_engine_process_interleaved_f32(engine_, input_stereo, output_stereo, frames);
}

fv1_sdk_result RealtimeProcessor::snapshot(fv1_sdk_snapshot_v1& out) const {
    if (!engine_) return FV1_SDK_ERROR_BAD_STATE;
    fv1_sdk_snapshot_v1_init(&out);
    return fv1_sdk_engine_get_snapshot_v1(engine_, &out);
}

} // namespace fv1::windows_frontend
