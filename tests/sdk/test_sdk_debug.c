#include <fv1/sdk.h>
#include <fv1/sdk_debug.h>

#include <stdio.h>
#include <string.h>

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
    unsigned char program[FV1_SDK_PROGRAM_BYTES];
    char diagnostic[256];
    if (fv1_sdk_compile_spinasm_v1(source, strlen(source), program, sizeof(program), NULL,
                                    diagnostic, sizeof(diagnostic)) != FV1_SDK_OK)
        return fail(diagnostic);

    fv1_sdk_engine_config_v1 config;
    fv1_sdk_engine_config_v1_init(&config);
    fv1_sdk_engine* engine = NULL;
    if (fv1_sdk_engine_create_v1(&config, &engine) != FV1_SDK_OK) return fail("create");
    if (fv1_sdk_engine_load_program(engine, program, sizeof(program)) != FV1_SDK_OK) return fail("load");
    if (fv1_sdk_debug_begin_sample(engine, 0.25f, -0.25f) != FV1_SDK_OK) return fail("begin sample");

    unsigned steps = 0;
    for (;;) {
        fv1_sdk_trace_v1 trace;
        fv1_sdk_trace_v1_init(&trace);
        if (fv1_sdk_debug_step_instruction_v1(engine, &trace) != FV1_SDK_OK) return fail("step");
        if (fv1_sdk_opcode_name(trace.opcode) == NULL) return fail("opcode name");
        if (strcmp(fv1_sdk_register_name(FV1_SDK_REG_POT0), "POT0") != 0 ||
            strcmp(fv1_sdk_register_name(FV1_SDK_REG0 + 31u), "REG31") != 0)
            return fail("register metadata");
        ++steps;
        if (trace.sample_finished) break;
        if (steps > 128u) return fail("sample never finished");
    }
    if (steps != 128u) return fail("unexpected instruction count");

    float out_l = 0.0f, out_r = 0.0f;
    if (fv1_sdk_debug_finish_sample(engine, &out_l, &out_r) != FV1_SDK_OK) return fail("finish sample");

    fv1_sdk_state_digest_v1 digest;
    fv1_sdk_state_digest_v1_init(&digest);
    if (fv1_sdk_engine_get_state_digest_v1(engine, &digest) != FV1_SDK_OK) return fail("digest");
    if (digest.architectural_hash == 0 || digest.delay_hash == 0 || digest.sample_counter != 1)
        return fail("invalid digest");

    fv1_sdk_engine_destroy(engine);
    printf("FV-1 SDK debug API passed (%u instructions, digest 0x%llx)\n",
           steps, (unsigned long long)digest.architectural_hash);
    return 0;
}
