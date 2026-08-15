#include <fv1/sdk.hpp>

#include <cmath>
#include <cstdio>

int main() {
    const auto compiled = fv1::sdk::compile_spinasm(
        "RDAX ADCL, 1.0\nWRAX DACL, 0\nRDAX ADCR, 1.0\nWRAX DACR, 0\n");
    if (!compiled || compiled.report.instruction_count != 4) return 1;

    fv1::sdk::Engine engine;
    if (engine.create() != FV1_SDK_OK || !engine.valid()) return 2;
    if (engine.load_program(compiled.program) != FV1_SDK_OK) return 3;
    if (engine.set_pot(0, 0.5f) != FV1_SDK_OK) return 4;

    float io[4] = {0.25f, -0.25f, 0.125f, -0.125f};
    if (engine.process_interleaved(io, io, 2) != FV1_SDK_OK) return 5;
    if (std::fabs(io[0] - 0.25f) > 2.0e-6f || std::fabs(io[1] + 0.25f) > 2.0e-6f) return 6;

    std::printf("FV-1 SDK C++ wrapper passed (%s)\n", fv1_sdk_get_version_string());
    return 0;
}
