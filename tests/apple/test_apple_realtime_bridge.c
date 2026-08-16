#include "../../src/apple/fv1_apple_realtime.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int nearly_equal(float a, float b, float epsilon) {
    return fabsf(a - b) <= epsilon;
}

static void phase8b_sleep_ms(long milliseconds) {
    const clock_t start = clock();
    const double target =
        (double)milliseconds / 1000.0;
    while (((double)(clock() - start) / (double)CLOCKS_PER_SEC) < target) {
        /* Standard-C bounded wait for the background analyzer worker. */
    }
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

    /*
     * Phase 8B: prove RAW + PROCESSED background analyzer snapshots with a
     * deterministic 1 kHz generator signal.
     */
    static const char passthrough_source[] =
        "RDAX ADCL, 1.0\n"
        "WRAX DACL, 0\n"
        "RDAX ADCR, 1.0\n"
        "WRAX DACR, 0\n";
    uint8_t passthrough_program[FV1_SDK_PROGRAM_BYTES];
    fv1_sdk_compile_report_v1 passthrough_report;
    char passthrough_diag[256];
    fv1_sdk_compile_report_v1_init(&passthrough_report);
    memset(passthrough_program, 0, sizeof(passthrough_program));
    memset(passthrough_diag, 0, sizeof(passthrough_diag));

    if (fv1_sdk_compile_spinasm_v1(
            passthrough_source,
            sizeof(passthrough_source) - 1u,
            passthrough_program,
            sizeof(passthrough_program),
            &passthrough_report,
            passthrough_diag,
            sizeof(passthrough_diag)) != FV1_SDK_OK ||
        fv1_apple_realtime_load_program(
            bridge,
            passthrough_program,
            sizeof(passthrough_program)) != FV1_SDK_OK ||
        fv1_apple_realtime_configure_rates(
            bridge,
            48000.0,
            48000.0) != FV1_SDK_OK) {
        fprintf(stderr, "Phase 8B analyzer setup failed: %s\n", passthrough_diag);
        fv1_apple_realtime_destroy(bridge);
        return 9;
    }

    fv1_apple_realtime_set_test_generator(
        bridge,
        FV1_APPLE_TEST_SIGNAL_SINE,
        1000.0,
        0.25,
        12000.0,
        5.0,
        1.0);

    for (int block = 0; block < 20; ++block) {
        if (fv1_apple_realtime_process_test_generator(
                bridge,
                FRAMES) != FV1_SDK_OK) {
            fprintf(stderr, "Phase 8B generator failed\n");
            fv1_apple_realtime_destroy(bridge);
            return 10;
        }
        fv1_apple_realtime_render_planar_output(
            bridge,
            out_left,
            out_right,
            FRAMES);
    }

    phase8b_sleep_ms(120);

    fv1_apple_analysis_snapshot_v1 raw_analysis;
    fv1_apple_analysis_snapshot_v1 processed_analysis;
    float raw_spectrum[2049];
    float processed_spectrum[2049];
    float raw_scope[512];
    float raw_scope_r[512];
    float processed_scope[512];
    float processed_scope_r[512];

    if (fv1_apple_realtime_copy_analysis(
            bridge,
            FV1_APPLE_ANALYSIS_RAW,
            &raw_analysis,
            raw_spectrum,
            2049u,
            raw_scope,
            raw_scope_r,
            512u) != FV1_SDK_OK ||
        fv1_apple_realtime_copy_analysis(
            bridge,
            FV1_APPLE_ANALYSIS_PROCESSED,
            &processed_analysis,
            processed_spectrum,
            2049u,
            processed_scope,
            processed_scope_r,
            512u) != FV1_SDK_OK ||
        raw_analysis.sequence == 0u ||
        processed_analysis.sequence == 0u ||
        raw_analysis.spectrum_bins != 2049u ||
        processed_analysis.scope_frames == 0u ||
        raw_analysis.dominant_frequency_hz < 970.0f ||
        raw_analysis.dominant_frequency_hz > 1030.0f ||
        raw_analysis.rms_left < 0.10f) {
        fprintf(
            stderr,
            "Phase 8B analyzer mismatch: raw seq=%llu f=%f rms=%f bins=%u; processed seq=%llu\n",
            (unsigned long long)raw_analysis.sequence,
            raw_analysis.dominant_frequency_hz,
            raw_analysis.rms_left,
            raw_analysis.spectrum_bins,
            (unsigned long long)processed_analysis.sequence);
        fv1_apple_realtime_destroy(bridge);
        return 11;
    }

    /*
     * DSP bypass routes raw input audibly while the processed FV-1 path stays
     * alive for simultaneous A/B analysis.
     */
    static const char mute_source[] =
        "RDAX ADCL, 0.0\n"
        "WRAX DACL, 0\n"
        "RDAX ADCR, 0.0\n"
        "WRAX DACR, 0\n";
    uint8_t mute_program[FV1_SDK_PROGRAM_BYTES];
    fv1_sdk_compile_report_v1 mute_report;
    char mute_diag[256];
    fv1_sdk_compile_report_v1_init(&mute_report);
    memset(mute_program, 0, sizeof(mute_program));
    memset(mute_diag, 0, sizeof(mute_diag));

    if (fv1_sdk_compile_spinasm_v1(
            mute_source,
            sizeof(mute_source) - 1u,
            mute_program,
            sizeof(mute_program),
            &mute_report,
            mute_diag,
            sizeof(mute_diag)) != FV1_SDK_OK ||
        fv1_apple_realtime_load_program(
            bridge,
            mute_program,
            sizeof(mute_program)) != FV1_SDK_OK ||
        fv1_apple_realtime_configure_rates(
            bridge,
            48000.0,
            48000.0) != FV1_SDK_OK) {
        fprintf(stderr, "Phase 8B bypass setup failed: %s\n", mute_diag);
        fv1_apple_realtime_destroy(bridge);
        return 12;
    }

    for (size_t i = 0u; i < FRAMES; ++i) {
        left[i] = 0.20f;
        right[i] = -0.10f;
    }

    fv1_apple_realtime_set_dsp_enabled(bridge, 0u);
    if (fv1_apple_realtime_get_dsp_enabled(bridge) != 0u ||
        fv1_apple_realtime_process_planar_input(
            bridge,
            left,
            right,
            FRAMES) != FV1_SDK_OK) {
        fprintf(stderr, "Phase 8B bypass enable failed\n");
        fv1_apple_realtime_destroy(bridge);
        return 13;
    }

    memset(out_left, 0, sizeof(out_left));
    memset(out_right, 0, sizeof(out_right));
    fv1_apple_realtime_render_planar_output(
        bridge,
        out_left,
        out_right,
        FRAMES);

    good = 0u;
    for (size_t i = 0u; i < FRAMES; ++i) {
        if (nearly_equal(out_left[i], 0.20f, 2.0e-5f) &&
            nearly_equal(out_right[i], -0.10f, 2.0e-5f)) {
            ++good;
        }
    }

    if (good < 470u) {
        fprintf(stderr, "Phase 8B bypass output mismatch: %zu good frames\n", good);
        fv1_apple_realtime_destroy(bridge);
        return 14;
    }

    fv1_apple_realtime_destroy(bridge);
    puts("Apple realtime SDK bridge + Phase 8B analyzer/bypass OK");
    return 0;
}
