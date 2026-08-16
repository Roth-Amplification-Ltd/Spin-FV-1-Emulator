#include "../../src/apple/fv1_apple_realtime.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int finite_block(
    const float* left,
    const float* right,
    size_t frames) {
    size_t i;
    for (i = 0u; i < frames; ++i) {
        if (!isfinite(left[i]) || !isfinite(right[i])) {
            return 0;
        }
    }
    return 1;
}

static unsigned long soak_seconds(void) {
    const char* text = getenv("FV1_APPLE_SOAK_SECONDS");
    char* end = NULL;
    unsigned long value;

    if (!text || text[0] == '\0') {
        return 30ul;
    }

    value = strtoul(text, &end, 10);
    if (!end || *end != '\0' || value == 0ul) {
        return 30ul;
    }

    return value;
}

int main(void) {
    static const char source[] =
        "RDAX ADCL, 1.0\n"
        "WRAX DACL, 0\n"
        "RDAX ADCR, 1.0\n"
        "WRAX DACR, 0\n";

    static const size_t fft_sizes[] = {
        1024u,
        2048u,
        4096u,
        8192u
    };

    uint8_t program[FV1_SDK_PROGRAM_BYTES];
    fv1_sdk_compile_report_v1 report;
    char diagnostic[512];

    fv1_apple_realtime* bridge = NULL;

    enum { FRAMES = 256 };
    float output_left[FRAMES];
    float output_right[FRAMES];

    const unsigned long seconds = soak_seconds();
    const time_t start = time(NULL);
    const time_t finish = start + (time_t)seconds;

    uint64_t blocks = 0u;
    uint64_t rendered_frames = 0u;

    memset(program, 0, sizeof(program));
    memset(diagnostic, 0, sizeof(diagnostic));
    fv1_sdk_compile_report_v1_init(&report);

    if (fv1_sdk_compile_spinasm_v1(
            source,
            sizeof(source) - 1u,
            program,
            sizeof(program),
            &report,
            diagnostic,
            sizeof(diagnostic)) != FV1_SDK_OK) {
        fprintf(stderr, "soak program compile failed: %s\n", diagnostic);
        return 1;
    }

    if (fv1_apple_realtime_create(
            48000.0,
            48000.0,
            32768u,
            &bridge) != FV1_SDK_OK
        || !bridge) {
        fprintf(stderr, "soak bridge create failed\n");
        return 2;
    }

    if (fv1_apple_realtime_load_program(
            bridge,
            program,
            sizeof(program)) != FV1_SDK_OK
        || fv1_apple_realtime_configure_rates(
            bridge,
            48000.0,
            48000.0) != FV1_SDK_OK) {
        fprintf(stderr, "soak bridge setup failed\n");
        fv1_apple_realtime_destroy(bridge);
        return 3;
    }

    printf(
        "Accelerated Apple bridge soak: %lu second(s)\n",
        seconds);

    while (time(NULL) < finish) {
        const size_t fft =
            fft_sizes[
                (blocks / 128u)
                % (sizeof(fft_sizes) / sizeof(fft_sizes[0]))];

        const uint32_t signal =
            (uint32_t)(
                (blocks / 32u)
                % 5u);

        const float pot0 =
            (float)((blocks % 101u) / 100.0);
        const float pot1 =
            (float)(((blocks * 3u) % 101u) / 100.0);
        const float pot2 =
            (float)(((blocks * 7u) % 101u) / 100.0);

        if ((blocks % 128u) == 0u) {
            if (fv1_apple_realtime_set_analyzer_fft_size(
                    bridge,
                    fft) != FV1_SDK_OK
                || fv1_apple_realtime_configure_rates(
                    bridge,
                    48000.0,
                    48000.0) != FV1_SDK_OK) {
                fprintf(stderr, "soak FFT/rate reconfigure failed\n");
                fv1_apple_realtime_destroy(bridge);
                return 4;
            }
        }

        fv1_apple_realtime_set_dsp_enabled(
            bridge,
            ((blocks / 64u) & 1u) == 0u ? 1u : 0u);

        fv1_apple_realtime_set_pots(
            bridge,
            pot0,
            pot1,
            pot2);

        fv1_apple_realtime_set_test_generator(
            bridge,
            signal,
            440.0 + (double)(blocks % 1200u),
            0.20,
            12000.0,
            5.0,
            1.0);

        if (fv1_apple_realtime_process_test_generator(
                bridge,
                FRAMES) != FV1_SDK_OK) {
            fprintf(stderr, "soak generator/process failure\n");
            fv1_apple_realtime_destroy(bridge);
            return 5;
        }

        memset(output_left, 0, sizeof(output_left));
        memset(output_right, 0, sizeof(output_right));

        fv1_apple_realtime_render_planar_output(
            bridge,
            output_left,
            output_right,
            FRAMES);

        if (!finite_block(
                output_left,
                output_right,
                FRAMES)) {
            fprintf(stderr, "soak detected non-finite audio\n");
            fv1_apple_realtime_destroy(bridge);
            return 6;
        }

        ++blocks;
        rendered_frames += FRAMES;
    }

    {
        fv1_apple_realtime_stats_v1 stats;
        fv1_apple_realtime_stats_v1_init(&stats);
        fv1_apple_realtime_get_stats(bridge, &stats);

        if (stats.last_sdk_result != FV1_SDK_OK
            || stats.chip_frames == 0u) {
            fprintf(
                stderr,
                "soak final stats invalid: chip=%llu last=%d\n",
                (unsigned long long)stats.chip_frames,
                (int)stats.last_sdk_result);
            fv1_apple_realtime_destroy(bridge);
            return 7;
        }

        printf(
            "PASS blocks=%llu rendered=%llu chip=%llu underflows=%llu overflows=%llu\n",
            (unsigned long long)blocks,
            (unsigned long long)rendered_frames,
            (unsigned long long)stats.chip_frames,
            (unsigned long long)stats.output_underflows,
            (unsigned long long)stats.output_overflows);
    }

    fv1_apple_realtime_destroy(bridge);
    return 0;
}
