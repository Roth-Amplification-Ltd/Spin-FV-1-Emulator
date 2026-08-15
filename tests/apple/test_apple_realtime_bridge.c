#include "../../src/apple/fv1_apple_realtime.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int nearly_equal(float a, float b, float epsilon) {
    return fabsf(a - b) <= epsilon;
}

int main(void) {
    fv1_apple_realtime* bridge = NULL;
    if (fv1_apple_realtime_create(48000.0, 48000.0, 4096u, &bridge) != FV1_SDK_OK || !bridge) {
        fprintf(stderr, "bridge creation failed\n");
        return 1;
    }

    enum { FRAMES = 480 };
    float left[FRAMES];
    float right[FRAMES];
    float out_left[FRAMES];
    float out_right[FRAMES];
    for (size_t i = 0u; i < FRAMES; ++i) {
        left[i] = 0.25f;
        right[i] = -0.125f;
    }

    fv1_apple_realtime_set_pots(bridge, 0.25f, 0.5f, 0.75f);
    if (fv1_apple_realtime_process_planar_input(bridge, left, right, FRAMES) != FV1_SDK_OK) {
        fprintf(stderr, "bridge processing failed\n");
        fv1_apple_realtime_destroy(bridge);
        return 2;
    }

    memset(out_left, 0, sizeof(out_left));
    memset(out_right, 0, sizeof(out_right));
    fv1_apple_realtime_render_planar_output(bridge, out_left, out_right, FRAMES);

    size_t good = 0u;
    for (size_t i = 0u; i < FRAMES; ++i) {
        if (nearly_equal(out_left[i], 0.25f, 2.0e-5f) &&
            nearly_equal(out_right[i], -0.125f, 2.0e-5f)) {
            ++good;
        }
    }
    if (good < 470u) {
        fprintf(stderr, "unexpected passthrough/resampler output: only %zu good frames\n", good);
        fv1_apple_realtime_destroy(bridge);
        return 3;
    }

    fv1_apple_realtime_stats_v1 stats;
    fv1_apple_realtime_get_stats(bridge, &stats);
    if (stats.input_frames != FRAMES || stats.chip_frames < 320u || stats.chip_frames > 335u ||
        stats.generated_output_frames < 470u || stats.last_sdk_result != FV1_SDK_OK) {
        fprintf(stderr,
                "unexpected stats: input=%llu chip=%llu generated=%llu last=%d\n",
                (unsigned long long)stats.input_frames,
                (unsigned long long)stats.chip_frames,
                (unsigned long long)stats.generated_output_frames,
                (int)stats.last_sdk_result);
        fv1_apple_realtime_destroy(bridge);
        return 4;
    }

    float scope_left[64];
    float scope_right[64];
    const size_t scope_frames = fv1_apple_realtime_copy_scope(bridge, scope_left, scope_right, 64u);
    if (scope_frames != 64u || !nearly_equal(scope_left[63], 0.25f, 2.0e-5f)) {
        fprintf(stderr, "scope copy failed\n");
        fv1_apple_realtime_destroy(bridge);
        return 5;
    }


    /* Prove atomic UI->audio POT handoff is applied by the audio owner thread. */
    static const char pot_source[] =
        "RDAX POT0, 1.0\n"
        "WRAX DACL, 0\n"
        "RDAX POT1, 1.0\n"
        "WRAX DACR, 0\n";
    uint8_t pot_program[FV1_SDK_PROGRAM_BYTES];
    fv1_sdk_compile_report_v1 pot_report;
    char pot_diag[256];
    fv1_sdk_compile_report_v1_init(&pot_report);
    memset(pot_program, 0, sizeof(pot_program));
    memset(pot_diag, 0, sizeof(pot_diag));
    if (fv1_sdk_compile_spinasm_v1(pot_source, sizeof(pot_source) - 1u,
                                    pot_program, sizeof(pot_program),
                                    &pot_report, pot_diag, sizeof(pot_diag)) != FV1_SDK_OK ||
        fv1_apple_realtime_load_program(bridge, pot_program, sizeof(pot_program)) != FV1_SDK_OK ||
        fv1_apple_realtime_configure_rates(bridge, 48000.0, 48000.0) != FV1_SDK_OK) {
        fprintf(stderr, "POT program setup failed: %s\n", pot_diag);
        fv1_apple_realtime_destroy(bridge);
        return 6;
    }
    memset(left, 0, sizeof(left));
    memset(right, 0, sizeof(right));
    fv1_apple_realtime_set_pots(bridge, 0.25f, 0.5f, 0.75f);
    if (fv1_apple_realtime_process_planar_input(bridge, left, right, FRAMES) != FV1_SDK_OK) {
        fprintf(stderr, "POT processing failed\n");
        fv1_apple_realtime_destroy(bridge);
        return 7;
    }
    memset(out_left, 0, sizeof(out_left));
    memset(out_right, 0, sizeof(out_right));
    fv1_apple_realtime_render_planar_output(bridge, out_left, out_right, FRAMES);
    good = 0u;
    for (size_t i = 0u; i < FRAMES; ++i) {
        if (nearly_equal(out_left[i], 0.25f, 5.0e-5f) &&
            nearly_equal(out_right[i], 0.5f, 5.0e-5f)) {
            ++good;
        }
    }
    if (good < 470u) {
        fprintf(stderr, "POT handoff output mismatch: only %zu good frames\n", good);
        fv1_apple_realtime_destroy(bridge);
        return 8;
    }

    fv1_apple_realtime_destroy(bridge);
    puts("Apple realtime SDK bridge OK");
    return 0;
}
