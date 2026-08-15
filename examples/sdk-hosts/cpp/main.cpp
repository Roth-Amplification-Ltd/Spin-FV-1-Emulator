#include <fv1/sdk.hpp>

#include <cmath>
#include <iostream>

int main() {
    auto compiled = fv1::sdk::compile_spinasm(
        "RDAX ADCL, 1.0\nWRAX DACL, 0\nRDAX ADCR, 1.0\nWRAX DACR, 0\n");
    if (!compiled) {
        std::cerr << compiled.diagnostic << '\n';
        return 2;
    }
    fv1::sdk::Engine engine;
    if (engine.create() != FV1_SDK_OK || engine.load_program(compiled.program) != FV1_SDK_OK) return 3;
    if (engine.set_pot(3, 0.5f) != FV1_SDK_ERROR_INVALID_ARGUMENT) return 4;
    engine.set_pots(0.25f, 0.5f, 0.75f);
    float audio[4] = {0.25f, -0.25f, 0.125f, -0.125f};
    if (engine.process_interleaved(audio, audio, 2) != FV1_SDK_OK) return 5;
    if (std::fabs(audio[0] - 0.25f) > 2e-6f) return 6;
    const auto [snap_result, snapshot] = engine.snapshot();
    if (snap_result != FV1_SDK_OK || snapshot.sample_counter != 2u) return 7;
    const auto [program_result, readback] = engine.program();
    if (program_result != FV1_SDK_OK || readback != compiled.program) return 8;
    std::cout << "C++ host OK: FV1SDK " << fv1_sdk_get_version_string()
              << ", sample " << snapshot.sample_counter << '\n';
    return 0;
}
