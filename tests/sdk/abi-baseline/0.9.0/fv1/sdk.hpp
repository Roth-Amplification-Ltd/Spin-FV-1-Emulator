#pragma once

#include <fv1/sdk.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace fv1::sdk {

using ProgramImage = std::array<std::uint8_t, FV1_SDK_PROGRAM_BYTES>;

struct CompileOutput {
    fv1_sdk_result result{FV1_SDK_ERROR_INTERNAL};
    ProgramImage program{};
    fv1_sdk_compile_report_v1 report{};
    std::string diagnostic;

    explicit operator bool() const noexcept { return result == FV1_SDK_OK; }
};

inline CompileOutput compile_spinasm(std::string_view source) {
    CompileOutput output;
    fv1_sdk_compile_report_v1_init(&output.report);
    std::array<char, 1024> diagnostic{};
    output.result = fv1_sdk_compile_spinasm_v1(
        source.data(), source.size(), output.program.data(), output.program.size(),
        &output.report, diagnostic.data(), diagnostic.size());
    output.diagnostic = diagnostic.data();
    return output;
}

class Engine {
public:
    Engine() = default;
    ~Engine() { reset_handle(); }

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    Engine(Engine&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    Engine& operator=(Engine&& other) noexcept {
        if (this != &other) {
            reset_handle();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    fv1_sdk_result create(const fv1_sdk_engine_config_v1* supplied = nullptr) noexcept {
        reset_handle();
        fv1_sdk_engine_config_v1 local{};
        if (!supplied) {
            fv1_sdk_engine_config_v1_init(&local);
            supplied = &local;
        }
        return fv1_sdk_engine_create_v1(supplied, &handle_);
    }

    bool valid() const noexcept { return handle_ != nullptr; }
    explicit operator bool() const noexcept { return valid(); }
    fv1_sdk_engine* get() noexcept { return handle_; }
    const fv1_sdk_engine* get() const noexcept { return handle_; }

    fv1_sdk_result reset(bool clear_delay_ram = true) noexcept {
        return fv1_sdk_engine_reset(handle_, clear_delay_ram ? 1 : 0);
    }

    fv1_sdk_result load_program(const ProgramImage& program) noexcept {
        return fv1_sdk_engine_load_program(handle_, program.data(), program.size());
    }

    std::pair<fv1_sdk_result, ProgramImage> program() const noexcept {
        ProgramImage image{};
        const auto result = fv1_sdk_engine_get_program(handle_, image.data(), image.size());
        return {result, image};
    }

    fv1_sdk_result set_pot(std::uint32_t index, float value) noexcept {
        return fv1_sdk_engine_set_pot(handle_, index, value);
    }

    fv1_sdk_result set_pots(float a, float b, float c) noexcept {
        return fv1_sdk_engine_set_pots(handle_, a, b, c);
    }

    fv1_sdk_result process_sample(float input_left, float input_right,
                                  float& output_left, float& output_right) noexcept {
        return fv1_sdk_engine_process_sample_f32(handle_, input_left, input_right,
                                                  &output_left, &output_right);
    }

    fv1_sdk_result process_planar(const float* input_left, const float* input_right,
                                  float* output_left, float* output_right,
                                  std::size_t frames) noexcept {
        return fv1_sdk_engine_process_planar_f32(handle_, input_left, input_right,
                                                  output_left, output_right, frames);
    }

    fv1_sdk_result process_interleaved(const float* input, float* output,
                                       std::size_t frames) noexcept {
        return fv1_sdk_engine_process_interleaved_f32(handle_, input, output, frames);
    }

    std::pair<fv1_sdk_result, fv1_sdk_snapshot_v1> snapshot() const noexcept {
        fv1_sdk_snapshot_v1 value{};
        fv1_sdk_snapshot_v1_init(&value);
        const auto result = fv1_sdk_engine_get_snapshot_v1(handle_, &value);
        return {result, value};
    }

    std::pair<fv1_sdk_result, fv1_sdk_resource_report_v1> resource_report() const noexcept {
        fv1_sdk_resource_report_v1 value{};
        fv1_sdk_resource_report_v1_init(&value);
        const auto result = fv1_sdk_engine_analyze_program_v1(handle_, &value);
        return {result, value};
    }

private:
    void reset_handle() noexcept {
        if (handle_) fv1_sdk_engine_destroy(handle_);
        handle_ = nullptr;
    }

    fv1_sdk_engine* handle_{};
};

} // namespace fv1::sdk
