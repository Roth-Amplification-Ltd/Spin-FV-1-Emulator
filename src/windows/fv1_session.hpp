#pragma once

#include <fv1/sdk.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fv1::windows_frontend {

struct CompileOutcome {
    fv1_sdk_result result{FV1_SDK_ERROR_INTERNAL};
    fv1_sdk_compile_report_v1 report{};
    std::string diagnostic;
};

class Session final {
public:
    Session();
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    [[nodiscard]] bool ready() const noexcept { return engine_ != nullptr; }
    [[nodiscard]] bool program_loaded() const noexcept { return program_loaded_; }
    [[nodiscard]] std::string_view last_error() const noexcept { return last_error_; }

    CompileOutcome compile_and_load(std::string_view source_utf8);
    fv1_sdk_result load_program(const std::uint8_t* bytes, std::size_t size);
    fv1_sdk_result reset(bool clear_delay_ram = true);
    fv1_sdk_result set_pot(std::uint32_t index, float value);

    // Runs a deterministic, inaudible sine probe through the public SDK. This
    // keeps Phase 7A useful as a native chip monitor before realtime WASAPI
    // streaming is introduced in the next Windows-audio increment.
    fv1_sdk_result run_probe(std::size_t frames, std::vector<float>& mono_output);

    fv1_sdk_result snapshot(fv1_sdk_snapshot_v1& out) const;
    fv1_sdk_result resources(fv1_sdk_resource_report_v1& out) const;

private:
    void remember_error(fv1_sdk_result result, std::string_view context);

    fv1_sdk_engine* engine_{};
    bool program_loaded_{};
    double probe_phase_{};
    double virtual_sample_rate_{32768.0};
    std::string last_error_;
};

} // namespace fv1::windows_frontend
