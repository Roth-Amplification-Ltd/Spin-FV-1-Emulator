#include "../../src/windows/realtime_processor.hpp"

#include <fv1/sdk.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace {

int fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

} // namespace

int main() {
    std::array<std::uint8_t, FV1_SDK_PROGRAM_BYTES> program{};
    fv1_sdk_compile_report_v1 report{};
    fv1_sdk_compile_report_v1_init(&report);
    constexpr std::string_view source =
        "RDAX ADCL, 1.0\n"
        "WRAX DACL, 0\n"
        "RDAX ADCR, 1.0\n"
        "WRAX DACR, 0\n";
    const auto compiled = fv1_sdk_compile_spinasm_v1(
        source.data(), source.size(), program.data(), program.size(), &report, nullptr, 0u);
    if (compiled != FV1_SDK_OK) return fail("could not compile realtime passthrough fixture");

    fv1::windows_frontend::RealtimeProcessor processor;
    if (!processor.ready()) return fail("could not create realtime SDK processor");
    if (processor.load_program(program) != FV1_SDK_OK) return fail("could not load realtime program");
    if (processor.set_pots({0.1F, 0.5F, 0.9F}) != FV1_SDK_OK) return fail("realtime POT update failed");

    constexpr std::size_t frames = 256u;
    std::array<float, frames * 2u> input{};
    std::array<float, frames * 2u> output{};
    for (std::size_t i = 0; i < frames; ++i) {
        const float sample = static_cast<float>(i) / static_cast<float>(frames);
        input[i * 2u] = sample;
        input[(i * 2u) + 1u] = -sample;
    }
    if (processor.process_interleaved(input.data(), output.data(), frames) != FV1_SDK_OK)
        return fail("realtime interleaved processing failed");
    for (float sample : output) {
        if (!std::isfinite(sample)) return fail("non-finite realtime output");
    }

    if (processor.reset(true) != FV1_SDK_OK) return fail("realtime processor reset failed");
    std::printf("Phase 7 realtime SDK processor OK: frames=%zu instructions=%u\n",
                frames, report.instruction_count);
    return 0;
}
