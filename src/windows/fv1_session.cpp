#include "fv1_session.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fv1::windows_frontend {
namespace {

constexpr std::size_t kDiagnosticCapacity = 8192u;
constexpr double kProbeFrequencyHz = 440.0;
constexpr float kProbeAmplitude = 0.20F;

} // namespace

Session::Session() {
    fv1_sdk_engine_config_v1 config{};
    fv1_sdk_engine_config_v1_init(&config);
    virtual_sample_rate_ = config.virtual_sample_rate;

    const auto result = fv1_sdk_engine_create_v1(&config, &engine_);
    if (result != FV1_SDK_OK) {
        remember_error(result, "create engine");
    }
}

Session::~Session() {
    if (engine_) {
        fv1_sdk_engine_destroy(engine_);
        engine_ = nullptr;
    }
}

CompileOutcome Session::compile_and_load(std::string_view source_utf8) {
    CompileOutcome outcome{};
    fv1_sdk_compile_report_v1_init(&outcome.report);

    std::array<std::uint8_t, FV1_SDK_PROGRAM_BYTES> program{};
    std::array<char, kDiagnosticCapacity> diagnostic{};
    outcome.result = fv1_sdk_compile_spinasm_v1(
        source_utf8.data(), source_utf8.size(),
        program.data(), program.size(),
        &outcome.report, diagnostic.data(), diagnostic.size());

    if (outcome.report.diagnostic_bytes_written != 0u) {
        const auto count = std::min<std::size_t>(
            outcome.report.diagnostic_bytes_written, diagnostic.size() - 1u);
        outcome.diagnostic.assign(diagnostic.data(), count);
    }

    if (outcome.result != FV1_SDK_OK) {
        remember_error(outcome.result, "compile SpinASM");
        return outcome;
    }

    outcome.result = load_program(program.data(), program.size());
    return outcome;
}

fv1_sdk_result Session::load_program(const std::uint8_t* bytes, std::size_t size) {
    if (!engine_) {
        remember_error(FV1_SDK_ERROR_BAD_STATE, "load program");
        return FV1_SDK_ERROR_BAD_STATE;
    }
    const auto result = fv1_sdk_engine_load_program(engine_, bytes, size);
    if (result == FV1_SDK_OK) {
        program_loaded_ = true;
        std::copy_n(bytes, program_image_.size(), program_image_.begin());
        probe_phase_ = 0.0;
        last_error_.clear();
    } else {
        remember_error(result, "load program");
    }
    return result;
}

fv1_sdk_result Session::reset(bool clear_delay_ram) {
    if (!engine_) return FV1_SDK_ERROR_BAD_STATE;
    const auto result = fv1_sdk_engine_reset(engine_, clear_delay_ram ? 1u : 0u);
    if (result == FV1_SDK_OK) {
        probe_phase_ = 0.0;
        last_error_.clear();
    } else {
        remember_error(result, "reset engine");
    }
    return result;
}

fv1_sdk_result Session::set_pot(std::uint32_t index, float value) {
    if (!engine_) return FV1_SDK_ERROR_BAD_STATE;
    if (index >= pot_values_.size()) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    value = std::clamp(value, 0.0F, 1.0F);
    const auto result = fv1_sdk_engine_set_pot(engine_, index, value);
    if (result == FV1_SDK_OK) pot_values_[index] = value;
    if (result != FV1_SDK_OK) remember_error(result, "set POT");
    return result;
}

fv1_sdk_result Session::run_probe(std::size_t frames, std::vector<float>& mono_output) {
    mono_output.clear();
    if (!engine_ || !program_loaded_) return FV1_SDK_ERROR_BAD_STATE;

    mono_output.resize(frames);
    const double phase_step = (2.0 * std::numbers::pi * kProbeFrequencyHz) / virtual_sample_rate_;
    for (std::size_t i = 0; i < frames; ++i) {
        const float input = kProbeAmplitude * static_cast<float>(std::sin(probe_phase_));
        float left = 0.0F;
        float right = 0.0F;
        const auto result = fv1_sdk_engine_process_sample_f32(engine_, input, input, &left, &right);
        if (result != FV1_SDK_OK) {
            mono_output.resize(i);
            remember_error(result, "process probe");
            return result;
        }
        mono_output[i] = (left + right) * 0.5F;
        probe_phase_ += phase_step;
        if (probe_phase_ >= 2.0 * std::numbers::pi) {
            probe_phase_ -= 2.0 * std::numbers::pi;
        }
    }
    return FV1_SDK_OK;
}

fv1_sdk_result Session::snapshot(fv1_sdk_snapshot_v1& out) const {
    if (!engine_) return FV1_SDK_ERROR_BAD_STATE;
    fv1_sdk_snapshot_v1_init(&out);
    return fv1_sdk_engine_get_snapshot_v1(engine_, &out);
}

fv1_sdk_result Session::resources(fv1_sdk_resource_report_v1& out) const {
    if (!engine_) return FV1_SDK_ERROR_BAD_STATE;
    fv1_sdk_resource_report_v1_init(&out);
    return fv1_sdk_engine_analyze_program_v1(engine_, &out);
}

void Session::remember_error(fv1_sdk_result result, std::string_view context) {
    last_error_.assign(context);
    last_error_ += ": ";
    last_error_ += fv1_sdk_result_string(result);
}

} // namespace fv1::windows_frontend
