#include <fv1/sdk.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    static const char program_source[] =
        "RDAX ADCL, 1.0\n"
        "WRAX DACL, 0\n"
        "RDAX ADCR, 1.0\n"
        "WRAX DACR, 0\n";

    uint8_t program[FV1_SDK_PROGRAM_BYTES];
    char diagnostic[256];
    fv1_sdk_compile_report_v1 compile_report;
    fv1_sdk_compile_report_v1_init(&compile_report);
    fv1_sdk_result result = fv1_sdk_compile_spinasm_v1(
        program_source, strlen(program_source), program, sizeof(program),
        &compile_report, diagnostic, sizeof(diagnostic));
    if (result != FV1_SDK_OK) {
        fprintf(stderr, "SpinASM: %s\n", diagnostic);
        return 2;
    }

    fv1_sdk_engine_config_v1 config;
    fv1_sdk_engine_config_v1_init(&config);
    fv1_sdk_engine* engine = NULL;
    result = fv1_sdk_engine_create_v1(&config, &engine);
    if (result != FV1_SDK_OK) {
        fprintf(stderr, "create: %s\n", fv1_sdk_result_string(result));
        return 3;
    }
    result = fv1_sdk_engine_load_program(engine, program, sizeof(program));
    if (result != FV1_SDK_OK) {
        fprintf(stderr, "load: %s\n", fv1_sdk_result_string(result));
        fv1_sdk_engine_destroy(engine);
        return 4;
    }

    float input[8] = {0.25f, -0.25f, 0.125f, -0.125f, 0.0f, 0.0f, -0.5f, 0.5f};
    float output[8] = {0};
    result = fv1_sdk_engine_process_interleaved_f32(engine, input, output, 4);
    if (result != FV1_SDK_OK) {
        fprintf(stderr, "process: %s\n", fv1_sdk_result_string(result));
        fv1_sdk_engine_destroy(engine);
        return 5;
    }
    if (fabsf(output[0] - input[0]) > 2.0e-6f || fabsf(output[1] - input[1]) > 2.0e-6f) {
        fprintf(stderr, "unexpected output: %.9f %.9f\n", output[0], output[1]);
        fv1_sdk_engine_destroy(engine);
        return 6;
    }

    fv1_sdk_snapshot_v1 snapshot;
    fv1_sdk_snapshot_v1_init(&snapshot);
    result = fv1_sdk_engine_get_snapshot_v1(engine, &snapshot);
    if (result != FV1_SDK_OK) {
        fv1_sdk_engine_destroy(engine);
        return 7;
    }

    printf("FV1SDK %s ABI %u.%u: %u SpinASM instructions, %llu samples, output %.3f/%.3f\n",
           fv1_sdk_get_version_string(),
           FV1_SDK_ABI_VERSION_MAJOR, FV1_SDK_ABI_VERSION_MINOR,
           compile_report.instruction_count,
           (unsigned long long)snapshot.sample_counter,
           output[0], output[1]);
    fv1_sdk_engine_destroy(engine);
    return 0;
}
