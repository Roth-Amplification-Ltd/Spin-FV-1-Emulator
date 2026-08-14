#include <fv1/debugger.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <unordered_set>

namespace {
std::array<std::uint8_t, FV1_PROGRAM_BYTES> make_program() {
    std::array<std::uint8_t, FV1_PROGRAM_BYTES> bytes{};
    // Canonical NOPs everywhere.
    for (std::size_t i = 0; i < FV1_PROGRAM_WORDS; ++i) {
        bytes[i * 4 + 3] = 0x11;
    }
    return bytes;
}
}

int main() {
    auto bytes = make_program();
    fv1::Debugger debugger;
    if (!debugger.load_program(bytes)) return 1;
    debugger.set_input(0.25f, -0.25f);

    fv1::DebugStep step{};
    if (!debugger.step_instruction(step)) return 2;
    if (step.trace.pc_before != 0 || step.trace.pc_after != 1) return 3;

    const std::unordered_set<std::uint32_t> breakpoints{3};
    if (!debugger.continue_until_breakpoint(breakpoints, step)) return 4;
    if (!step.breakpoint_hit || step.snapshot.program_counter != 3) return 5;

    if (!debugger.step_sample(step)) return 6;
    if (!step.sample_finished || debugger.sample_index() != 1) return 7;

    std::puts("fv1-debugger tests passed");
    return 0;
}
