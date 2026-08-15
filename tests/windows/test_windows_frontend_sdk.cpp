#include "../../src/windows/fv1_session.hpp"

#include <fv1/sdk.h>

#include <cmath>
#include <cstdio>
#include <string_view>
#include <vector>

namespace {

int fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

} // namespace

int main() {
    using fv1::windows_frontend::Session;
    Session session;
    if (!session.ready()) return fail("native frontend session could not create SDK engine");

    constexpr std::string_view source =
        "RDAX ADCL, 1.0\n"
        "WRAX DACL, 0\n"
        "RDAX ADCR, 1.0\n"
        "WRAX DACR, 0\n";
    const auto compiled = session.compile_and_load(source);
    if (compiled.result != FV1_SDK_OK) return fail("native frontend session could not compile/load passthrough");
    if (compiled.report.instruction_count != 4u) return fail("unexpected passthrough instruction count");

    for (std::uint32_t i = 0; i < 3u; ++i) {
        if (session.set_pot(i, 0.25F * static_cast<float>(i + 1u)) != FV1_SDK_OK)
            return fail("POT update through SDK failed");
    }

    std::vector<float> output;
    if (session.run_probe(512u, output) != FV1_SDK_OK || output.size() != 512u)
        return fail("deterministic native frontend probe failed");
    for (float sample : output) {
        if (!std::isfinite(sample)) return fail("non-finite sample from frontend probe");
    }

    fv1_sdk_snapshot_v1 snapshot{};
    if (session.snapshot(snapshot) != FV1_SDK_OK || snapshot.sample_counter < 512u)
        return fail("native frontend snapshot did not advance");

    fv1_sdk_resource_report_v1 report{};
    if (session.resources(report) != FV1_SDK_OK || report.used_instructions != 4u)
        return fail("native frontend resource inspection failed");

    if (session.reset(true) != FV1_SDK_OK) return fail("native frontend reset failed");

    std::printf("Phase 7A native frontend SDK session OK: version=%s samples=%llu instructions=%u\n",
                fv1_sdk_get_version_string(),
                static_cast<unsigned long long>(snapshot.sample_counter),
                report.used_instructions);
    return 0;
}
