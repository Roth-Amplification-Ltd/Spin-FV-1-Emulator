#include <fv1/sdk.h>
#include <fv1/sdk_debug.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int fail(const char* message) {
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

static int expect_result(fv1_sdk_result got, fv1_sdk_result expected, const char* what) {
    if (got != expected) {
        fprintf(stderr, "FAIL: %s: got %d (%s), expected %d (%s)\n",
                what, (int)got, fv1_sdk_result_string(got),
                (int)expected, fv1_sdk_result_string(expected));
        return 1;
    }
    return 0;
}

int main(void) {
    static const char passthrough_source[] =
        "RDAX ADCL, 1.0\n"
        "WRAX DACL, 0\n"
        "RDAX ADCR, 1.0\n"
        "WRAX DACR, 0\n";
    uint8_t program[FV1_SDK_PROGRAM_BYTES];
    char diagnostic[32];

    /* Null/undersized/wrong-major versioned structures must fail cleanly. */
    if (expect_result(fv1_sdk_get_version_info_v1(NULL), FV1_SDK_ERROR_INVALID_ARGUMENT,
                      "null version-info") != 0) return 1;
    {
        fv1_sdk_version_info_v1 info;
        fv1_sdk_version_info_v1_init(&info);
        info.struct_size = 4;
        if (expect_result(fv1_sdk_get_version_info_v1(&info), FV1_SDK_ERROR_UNSUPPORTED,
                          "undersized version-info") != 0) return 1;
        fv1_sdk_version_info_v1_init(&info);
        info.abi_version = (FV1_SDK_ABI_VERSION_MAJOR + 1u) << 16;
        if (expect_result(fv1_sdk_get_version_info_v1(&info), FV1_SDK_ERROR_UNSUPPORTED,
                          "wrong-major version-info") != 0) return 1;
    }

    /* Config validation: no output handle on error and no non-finite clocks. */
    {
        fv1_sdk_engine_config_v1 config;
        fv1_sdk_engine* engine = (fv1_sdk_engine*)(uintptr_t)1u;
        fv1_sdk_engine_config_v1_init(&config);
        if (expect_result(fv1_sdk_engine_create_v1(NULL, &engine), FV1_SDK_ERROR_INVALID_ARGUMENT,
                          "null config") != 0) return 1;
        if (expect_result(fv1_sdk_engine_create_v1(&config, NULL), FV1_SDK_ERROR_INVALID_ARGUMENT,
                          "null output handle") != 0) return 1;

        config.struct_size = 4;
        if (expect_result(fv1_sdk_engine_create_v1(&config, &engine), FV1_SDK_ERROR_UNSUPPORTED,
                          "undersized config") != 0) return 1;
        if (engine != NULL) return fail("engine output was not cleared on config failure");

        fv1_sdk_engine_config_v1_init(&config);
        config.abi_version = (FV1_SDK_ABI_VERSION_MAJOR + 1u) << 16;
        if (expect_result(fv1_sdk_engine_create_v1(&config, &engine), FV1_SDK_ERROR_UNSUPPORTED,
                          "wrong-major config") != 0) return 1;

        fv1_sdk_engine_config_v1_init(&config);
        config.virtual_sample_rate = 0.0;
        if (expect_result(fv1_sdk_engine_create_v1(&config, &engine), FV1_SDK_ERROR_INVALID_ARGUMENT,
                          "zero sample rate") != 0) return 1;
        config.virtual_sample_rate = NAN;
        if (expect_result(fv1_sdk_engine_create_v1(&config, &engine), FV1_SDK_ERROR_INVALID_ARGUMENT,
                          "NaN sample rate") != 0) return 1;
        config.virtual_sample_rate = INFINITY;
        if (expect_result(fv1_sdk_engine_create_v1(&config, &engine), FV1_SDK_ERROR_INVALID_ARGUMENT,
                          "infinite sample rate") != 0) return 1;
        fv1_sdk_engine_config_v1_init(&config);
        config.delay_model = 99u;
        if (expect_result(fv1_sdk_engine_create_v1(&config, &engine), FV1_SDK_ERROR_INVALID_ARGUMENT,
                          "invalid delay model") != 0) return 1;
    }

    /* Compiler must reject bad arguments and expose deterministic truncation metadata. */
    {
        fv1_sdk_compile_report_v1 report;
        fv1_sdk_compile_report_v1_init(&report);
        if (expect_result(fv1_sdk_compile_spinasm_v1(NULL, 0, program, sizeof(program),
                                                     &report, diagnostic, sizeof(diagnostic)),
                          FV1_SDK_ERROR_INVALID_ARGUMENT, "null SpinASM source") != 0) return 1;
        if (report.diagnostic_bytes_required == 0) return fail("compiler argument error omitted diagnostic size");

        fv1_sdk_compile_report_v1_init(&report);
        report.struct_size = 4;
        if (expect_result(fv1_sdk_compile_spinasm_v1(passthrough_source,
                                                     strlen(passthrough_source), program,
                                                     sizeof(program), &report,
                                                     diagnostic, sizeof(diagnostic)),
                          FV1_SDK_ERROR_UNSUPPORTED, "undersized compile report") != 0) return 1;

        fv1_sdk_compile_report_v1_init(&report);
        if (expect_result(fv1_sdk_compile_spinasm_v1(passthrough_source,
                                                     strlen(passthrough_source), program,
                                                     FV1_SDK_PROGRAM_BYTES - 1u, &report,
                                                     diagnostic, sizeof(diagnostic)),
                          FV1_SDK_ERROR_INVALID_ARGUMENT, "short compiler output buffer") != 0) return 1;
    }

    {
        fv1_sdk_compile_report_v1 report;
        fv1_sdk_compile_report_v1_init(&report);
        if (fv1_sdk_compile_spinasm_v1(passthrough_source, strlen(passthrough_source),
                                       program, sizeof(program), &report,
                                       diagnostic, sizeof(diagnostic)) != FV1_SDK_OK)
            return fail("failed to compile abuse-test passthrough program");
    }

    /* Repeated lifetime creation/destruction should be boring and deterministic. */
    for (unsigned i = 0; i < 64u; ++i) {
        fv1_sdk_engine_config_v1 config;
        fv1_sdk_engine* e = NULL;
        fv1_sdk_engine_config_v1_init(&config);
        if (fv1_sdk_engine_create_v1(&config, &e) != FV1_SDK_OK || !e)
            return fail("repeated engine create failed");
        fv1_sdk_engine_destroy(e);
    }
    fv1_sdk_engine_destroy(NULL);

    fv1_sdk_engine_config_v1 config;
    fv1_sdk_engine_config_v1_init(&config);
    fv1_sdk_engine* engine = NULL;
    if (fv1_sdk_engine_create_v1(&config, &engine) != FV1_SDK_OK || !engine)
        return fail("engine creation failed");

    /* Processing and inspection before a program is loaded must not crash. */
    {
        float l = 0.0f, r = 0.0f;
        if (expect_result(fv1_sdk_engine_process_sample_f32(engine, 0.0f, 0.0f, &l, &r),
                          FV1_SDK_ERROR_BAD_STATE, "process before program") != 0) return 1;
        if (expect_result(fv1_sdk_debug_begin_sample(engine, 0.0f, 0.0f),
                          FV1_SDK_ERROR_BAD_STATE, "debug begin before program") != 0) return 1;
    }

    if (expect_result(fv1_sdk_engine_load_program(engine, program, FV1_SDK_PROGRAM_BYTES - 1u),
                      FV1_SDK_ERROR_INVALID_ARGUMENT, "short program") != 0) return 1;
    if (expect_result(fv1_sdk_engine_load_program(engine, NULL, FV1_SDK_PROGRAM_BYTES),
                      FV1_SDK_ERROR_INVALID_ARGUMENT, "null program") != 0) return 1;
    if (fv1_sdk_engine_load_program(engine, program, sizeof(program)) != FV1_SDK_OK)
        return fail("valid program load failed");

    /* POT API rejects non-finite values, rejects invalid indices, and clamps finite values. */
    if (expect_result(fv1_sdk_engine_set_pot(engine, 3u, 0.5f),
                      FV1_SDK_ERROR_INVALID_ARGUMENT, "invalid POT index") != 0) return 1;
    if (expect_result(fv1_sdk_engine_set_pot(engine, 0u, NAN),
                      FV1_SDK_ERROR_INVALID_ARGUMENT, "NaN POT") != 0) return 1;
    if (expect_result(fv1_sdk_engine_set_pot(engine, 0u, INFINITY),
                      FV1_SDK_ERROR_INVALID_ARGUMENT, "infinite POT") != 0) return 1;
    if (expect_result(fv1_sdk_engine_set_pots(engine, 0.5f, NAN, 0.5f),
                      FV1_SDK_ERROR_INVALID_ARGUMENT, "NaN POT triplet") != 0) return 1;
    if (fv1_sdk_engine_set_pots(engine, -100.0f, 0.5f, 100.0f) != FV1_SDK_OK)
        return fail("finite out-of-range POT triplet was not clamped");

    /* Zero-frame processing is a defined no-op and therefore needs no buffers. */
    if (fv1_sdk_engine_process_planar_f32(engine, NULL, NULL, NULL, NULL, 0) != FV1_SDK_OK)
        return fail("zero-frame planar processing was not a no-op");
    if (fv1_sdk_engine_process_interleaved_f32(engine, NULL, NULL, 0) != FV1_SDK_OK)
        return fail("zero-frame interleaved processing was not a no-op");
    if (expect_result(fv1_sdk_engine_process_planar_f32(engine, NULL, NULL, NULL, NULL, 1),
                      FV1_SDK_ERROR_INVALID_ARGUMENT, "null nonzero planar buffers") != 0) return 1;
    if (expect_result(fv1_sdk_engine_process_interleaved_f32(engine, NULL, NULL, 1),
                      FV1_SDK_ERROR_INVALID_ARGUMENT, "null nonzero interleaved buffers") != 0) return 1;

    /* Host-side NaN/Inf input is converted deterministically; never leak NaN/Inf from the core. */
    {
        float out_l = NAN, out_r = NAN;
        if (fv1_sdk_engine_process_sample_f32(engine, NAN, INFINITY, &out_l, &out_r) != FV1_SDK_OK)
            return fail("non-finite input processing failed");
        if (!isfinite(out_l) || !isfinite(out_r)) return fail("non-finite input produced non-finite output");
    }

    /* Versioned output structures reject malformed ABI headers. */
    {
        fv1_sdk_snapshot_v1 snapshot;
        fv1_sdk_snapshot_v1_init(&snapshot);
        snapshot.struct_size = 4;
        if (expect_result(fv1_sdk_engine_get_snapshot_v1(engine, &snapshot),
                          FV1_SDK_ERROR_UNSUPPORTED, "undersized snapshot") != 0) return 1;

        fv1_sdk_resource_report_v1 report;
        fv1_sdk_resource_report_v1_init(&report);
        report.abi_version = (FV1_SDK_ABI_VERSION_MAJOR + 1u) << 16;
        if (expect_result(fv1_sdk_engine_analyze_program_v1(engine, &report),
                          FV1_SDK_ERROR_UNSUPPORTED, "wrong-major resource report") != 0) return 1;

        fv1_sdk_state_digest_v1 digest;
        fv1_sdk_state_digest_v1_init(&digest);
        digest.struct_size = 4;
        if (expect_result(fv1_sdk_engine_get_state_digest_v1(engine, &digest),
                          FV1_SDK_ERROR_UNSUPPORTED, "undersized state digest") != 0) return 1;
    }

    {
        int32_t word = 0;
        if (expect_result(fv1_sdk_engine_read_delay_word(engine, FV1_SDK_DELAY_WORDS, &word),
                          FV1_SDK_ERROR_INVALID_ARGUMENT, "delay address out of range") != 0) return 1;
        if (expect_result(fv1_sdk_engine_read_delay_word(engine, 0, NULL),
                          FV1_SDK_ERROR_INVALID_ARGUMENT, "null delay output") != 0) return 1;
    }

    /* Debug state machine misuse must be rejected rather than silently advancing. */
    {
        fv1_sdk_trace_v1 trace;
        float l = 0.0f, r = 0.0f;
        fv1_sdk_trace_v1_init(&trace);
        if (expect_result(fv1_sdk_debug_step_instruction_v1(engine, &trace),
                          FV1_SDK_ERROR_BAD_STATE, "debug step without begin") != 0) return 1;
        if (expect_result(fv1_sdk_debug_finish_sample(engine, &l, &r),
                          FV1_SDK_ERROR_BAD_STATE, "debug finish without begin") != 0) return 1;
        if (fv1_sdk_debug_begin_sample(engine, 0.0f, 0.0f) != FV1_SDK_OK)
            return fail("debug begin failed");
        if (expect_result(fv1_sdk_debug_begin_sample(engine, 0.0f, 0.0f),
                          FV1_SDK_ERROR_BAD_STATE, "double debug begin") != 0) return 1;
        fv1_sdk_trace_v1_init(&trace);
        trace.struct_size = 4;
        if (expect_result(fv1_sdk_debug_step_instruction_v1(engine, &trace),
                          FV1_SDK_ERROR_UNSUPPORTED, "undersized trace") != 0) return 1;
        /* Complete the active sample with valid traces so the engine returns to normal state. */
        for (unsigned i = 0; i < 128u; ++i) {
            fv1_sdk_trace_v1_init(&trace);
            if (fv1_sdk_debug_step_instruction_v1(engine, &trace) != FV1_SDK_OK)
                return fail("valid debug step failed after malformed trace");
            if (trace.sample_finished) break;
        }
        if (fv1_sdk_debug_finish_sample(engine, &l, &r) != FV1_SDK_OK)
            return fail("valid debug finish failed");
    }

    fv1_sdk_engine_destroy(engine);
    puts("FV-1 SDK abuse/contract test passed");
    return 0;
}
