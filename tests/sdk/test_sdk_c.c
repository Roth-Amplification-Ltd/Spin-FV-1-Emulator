#include <fv1/sdk.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(fv1_sdk_result) == 4, "fv1_sdk_result must remain 32-bit");
_Static_assert(sizeof(fv1_sdk_delay_model) == 4, "fv1_sdk_delay_model must remain 32-bit");

static int fail(const char* message) {
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(void) {
    static const char source[] =
        "RDAX ADCL, 1.0\n"
        "WRAX DACL, 0\n"
        "RDAX ADCR, 1.0\n"
        "WRAX DACR, 0\n";

    fv1_sdk_version_info_v1 version;
    fv1_sdk_version_info_v1_init(&version);
    if (fv1_sdk_get_version_info_v1(&version) != FV1_SDK_OK) return fail("version info failed");
    if (version.abi_version != FV1_SDK_ABI_VERSION || version.program_bytes != FV1_SDK_PROGRAM_BYTES)
        return fail("version constants mismatch");
    if ((version.capabilities & FV1_SDK_CAP_CORE_PROCESSING) == 0 ||
        (version.capabilities & FV1_SDK_CAP_SPINASM_COMPILER) == 0 ||
        (version.capabilities & FV1_SDK_CAP_DEBUG_STEPPING) == 0)
        return fail("required capabilities missing");
    if (version.capabilities != fv1_sdk_get_capabilities()) return fail("capability discovery mismatch");

    uint8_t program[FV1_SDK_PROGRAM_BYTES];
    char diagnostic[256];
    fv1_sdk_compile_report_v1 compile_report;
    fv1_sdk_compile_report_v1_init(&compile_report);
    if (fv1_sdk_compile_spinasm_v1(source, strlen(source), program, sizeof(program),
                                    &compile_report, diagnostic, sizeof(diagnostic)) != FV1_SDK_OK)
        return fail(diagnostic);
    if (compile_report.instruction_count != 4) return fail("unexpected native compile count");

    {
        static const char bad_source[] = "NOPE REG0, 1.0\n";
        uint8_t bad_program[FV1_SDK_PROGRAM_BYTES];
        fv1_sdk_compile_report_v1 bad_report;
        fv1_sdk_compile_report_v1_init(&bad_report);
        diagnostic[0] = '\0';
        if (fv1_sdk_compile_spinasm_v1(bad_source, strlen(bad_source), bad_program, sizeof(bad_program),
                                        &bad_report, diagnostic, sizeof(diagnostic)) != FV1_SDK_ERROR_COMPILE)
            return fail("invalid SpinASM did not produce compile error");
        if (bad_report.error_line != 1 || diagnostic[0] == '\0') return fail("compile diagnostic metadata missing");
        if (bad_report.diagnostic_bytes_required <= 1 ||
            bad_report.diagnostic_bytes_written != strlen(diagnostic))
            return fail("compile diagnostic size metadata missing");
    }

    {
        static const char bad_source[] = "NOPE REG0, 1.0\n";
        uint8_t bad_program[FV1_SDK_PROGRAM_BYTES];
        char tiny[4];
        fv1_sdk_compile_report_v1 bad_report;
        fv1_sdk_compile_report_v1_init(&bad_report);
        if (fv1_sdk_compile_spinasm_v1(bad_source, strlen(bad_source), bad_program, sizeof(bad_program),
                                        &bad_report, tiny, sizeof(tiny)) != FV1_SDK_ERROR_COMPILE)
            return fail("tiny diagnostic compile did not fail predictably");
        if (bad_report.diagnostic_bytes_required <= sizeof(tiny) ||
            bad_report.diagnostic_bytes_written != sizeof(tiny) - 1 || tiny[sizeof(tiny)-1] != '\0')
            return fail("diagnostic truncation contract failed");
    }

    fv1_sdk_engine_config_v1 config;
    fv1_sdk_engine_config_v1_init(&config);
    config.virtual_sample_rate = 32768.0;

    fv1_sdk_engine* engine = NULL;
    if (fv1_sdk_engine_create_v1(&config, &engine) != FV1_SDK_OK || engine == NULL)
        return fail("engine creation failed");
    if (fv1_sdk_engine_load_program(engine, program, sizeof(program)) != FV1_SDK_OK)
        return fail("program load failed");
    if (fv1_sdk_engine_set_pots(engine, 0.1f, 0.5f, 0.9f) != FV1_SDK_OK)
        return fail("POT update failed");
    if (fv1_sdk_engine_set_pot(engine, 1, 0.75f) != FV1_SDK_OK)
        return fail("single POT update failed");
    if (fv1_sdk_engine_set_pot(engine, 3, 0.5f) != FV1_SDK_ERROR_INVALID_ARGUMENT)
        return fail("invalid POT index accepted");

    {
        uint8_t roundtrip[FV1_SDK_PROGRAM_BYTES];
        if (fv1_sdk_engine_get_program(engine, roundtrip, sizeof(roundtrip)) != FV1_SDK_OK)
            return fail("program readback failed");
        if (memcmp(roundtrip, program, sizeof(program)) != 0) return fail("program readback mismatch");
    }

    {
        float out_l = 0.0f, out_r = 0.0f;
        if (fv1_sdk_engine_process_sample_f32(engine, 0.375f, -0.375f, &out_l, &out_r) != FV1_SDK_OK)
            return fail("single-sample processing failed");
        if (fabsf(out_l - 0.375f) > 2.0e-6f || fabsf(out_r + 0.375f) > 2.0e-6f)
            return fail("single-sample passthrough mismatch");
    }

    {
        float in_l[4] = {0.25f, -0.125f, 0.0f, 0.5f};
        float in_r[4] = {-0.25f, 0.125f, 0.0f, -0.5f};
        float out_l[4] = {0};
        float out_r[4] = {0};
        if (fv1_sdk_engine_process_planar_f32(engine, in_l, in_r, out_l, out_r, 4) != FV1_SDK_OK)
            return fail("planar processing failed");
        if (fabsf(out_l[0] - in_l[0]) > 2.0e-6f || fabsf(out_r[0] - in_r[0]) > 2.0e-6f)
            return fail("planar passthrough mismatch");
    }

    {
        float stereo[4] = {0.125f, -0.125f, 0.25f, -0.25f};
        if (fv1_sdk_engine_process_interleaved_f32(engine, stereo, stereo, 2) != FV1_SDK_OK)
            return fail("in-place interleaved processing failed");
        if (fabsf(stereo[0] - 0.125f) > 2.0e-6f || fabsf(stereo[1] + 0.125f) > 2.0e-6f)
            return fail("interleaved passthrough mismatch");
    }

    {
        fv1_sdk_snapshot_v1 snapshot;
        fv1_sdk_snapshot_v1_init(&snapshot);
        if (fv1_sdk_engine_get_snapshot_v1(engine, &snapshot) != FV1_SDK_OK)
            return fail("snapshot failed");
        if (snapshot.sample_counter != 7) return fail("sample counter mismatch");
    }

    {
        fv1_sdk_resource_report_v1 report;
        fv1_sdk_resource_report_v1_init(&report);
        if (fv1_sdk_engine_analyze_program_v1(engine, &report) != FV1_SDK_OK)
            return fail("resource analysis failed");
        if (report.used_instructions != 4) return fail("resource instruction count mismatch");
    }

    if (fv1_sdk_get_abi_version() != FV1_SDK_ABI_VERSION) return fail("ABI version mismatch");
    if (fv1_sdk_get_version_string() == NULL || fv1_sdk_get_version_string()[0] == '\0')
        return fail("missing SDK version string");

    fv1_sdk_engine_destroy(engine);
    printf("FV-1 SDK C ABI test passed (%s, ABI %u.%u, caps 0x%llx)\n",
           fv1_sdk_get_version_string(), FV1_SDK_ABI_VERSION_MAJOR, FV1_SDK_ABI_VERSION_MINOR,
           (unsigned long long)version.capabilities);
    return 0;
}
