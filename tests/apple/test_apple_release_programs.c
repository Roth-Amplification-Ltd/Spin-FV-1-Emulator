#include "../../src/apple/fv1_apple_realtime.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct demo_case {
    const char* relative_path;
    const char* label;
} demo_case;

static const demo_case k_demos[] = {
    {"examples/simple_passthrough.spn", "Simple Passthrough"},
    {"examples/steal-this-dsp-programs/00_55_gallon_saint.spn", "55 Gallon Saint"},
    {"examples/steal-this-dsp-programs/01_last_known_copy.spn", "Last Known Copy"},
    {"examples/steal-this-dsp-programs/02_ghost_spring.spn", "Ghost Spring"},
    {"examples/steal-this-dsp-programs/03_gravity_clerk.spn", "Gravity Clerk"},
    {"examples/steal-this-dsp-programs/04_cold_case.spn", "Cold Case"},
    {"examples/steal-this-dsp-programs/05_municipal_lung.spn", "Municipal Lung"},
    {"examples/steal-this-dsp-programs/06_reverse_witness.spn", "Reverse Witness"},
    {"examples/steal-this-dsp-programs/07_data_felon.spn", "Data Felon"},
};

static char* read_text_file(const char* path, size_t* out_size) {
    FILE* file = fopen(path, "rb");
    long length;
    char* data;

    if (!file) {
        fprintf(stderr, "unable to open %s\n", path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    data = (char*)malloc((size_t)length + 1u);
    if (!data) {
        fclose(file);
        return NULL;
    }

    if (fread(data, 1u, (size_t)length, file) != (size_t)length) {
        free(data);
        fclose(file);
        return NULL;
    }

    fclose(file);
    data[length] = '\0';
    *out_size = (size_t)length;
    return data;
}

static int output_is_finite(
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

static int exercise_program(
    const char* root,
    const demo_case* demo) {
    char path[4096];
    char diagnostic[1024];
    char* source = NULL;
    size_t source_size = 0u;
    uint8_t program[FV1_SDK_PROGRAM_BYTES];
    fv1_sdk_compile_report_v1 report;
    fv1_apple_realtime* bridge = NULL;

    enum { FRAMES = 256 };
    float output_left[FRAMES];
    float output_right[FRAMES];

    static const float pot_sets[][3] = {
        {0.0f, 0.0f, 0.0f},
        {0.25f, 0.50f, 0.75f},
        {1.0f, 1.0f, 1.0f},
    };

    size_t pot_index;
    int block;

    if (snprintf(
            path,
            sizeof(path),
            "%s/%s",
            root,
            demo->relative_path) >= (int)sizeof(path)) {
        fprintf(stderr, "path too long for %s\n", demo->label);
        return 1;
    }

    source = read_text_file(path, &source_size);
    if (!source) {
        fprintf(stderr, "FAIL  %-20s  source read\n", demo->label);
        return 1;
    }

    memset(program, 0, sizeof(program));
    memset(diagnostic, 0, sizeof(diagnostic));
    fv1_sdk_compile_report_v1_init(&report);

    if (fv1_sdk_compile_spinasm_v1(
            source,
            source_size,
            program,
            sizeof(program),
            &report,
            diagnostic,
            sizeof(diagnostic)) != FV1_SDK_OK) {
        fprintf(
            stderr,
            "FAIL  %-20s  native compile: %s\n",
            demo->label,
            diagnostic);
        free(source);
        return 1;
    }

    free(source);
    source = NULL;

    if (fv1_apple_realtime_create(
            48000.0,
            48000.0,
            16384u,
            &bridge) != FV1_SDK_OK
        || !bridge) {
        fprintf(stderr, "FAIL  %-20s  bridge create\n", demo->label);
        return 1;
    }

    if (fv1_apple_realtime_load_program(
            bridge,
            program,
            sizeof(program)) != FV1_SDK_OK
        || fv1_apple_realtime_configure_rates(
            bridge,
            48000.0,
            48000.0) != FV1_SDK_OK) {
        fprintf(stderr, "FAIL  %-20s  bridge load/configure\n", demo->label);
        fv1_apple_realtime_destroy(bridge);
        return 1;
    }

    fv1_apple_realtime_set_test_generator(
        bridge,
        FV1_APPLE_TEST_SIGNAL_SINE,
        997.0,
        0.20,
        12000.0,
        5.0,
        1.0);

    for (pot_index = 0u;
         pot_index < sizeof(pot_sets) / sizeof(pot_sets[0]);
         ++pot_index) {
        fv1_apple_realtime_set_pots(
            bridge,
            pot_sets[pot_index][0],
            pot_sets[pot_index][1],
            pot_sets[pot_index][2]);

        for (block = 0; block < 24; ++block) {
            memset(output_left, 0, sizeof(output_left));
            memset(output_right, 0, sizeof(output_right));

            if (fv1_apple_realtime_process_test_generator(
                    bridge,
                    FRAMES) != FV1_SDK_OK) {
                fprintf(
                    stderr,
                    "FAIL  %-20s  process block %d\n",
                    demo->label,
                    block);
                fv1_apple_realtime_destroy(bridge);
                return 1;
            }

            fv1_apple_realtime_render_planar_output(
                bridge,
                output_left,
                output_right,
                FRAMES);

            if (!output_is_finite(
                    output_left,
                    output_right,
                    FRAMES)) {
                fprintf(
                    stderr,
                    "FAIL  %-20s  non-finite output\n",
                    demo->label);
                fv1_apple_realtime_destroy(bridge);
                return 1;
            }
        }
    }

    {
        fv1_apple_realtime_stats_v1 stats;
        fv1_apple_realtime_stats_v1_init(&stats);
        fv1_apple_realtime_get_stats(bridge, &stats);

        if (stats.chip_frames == 0u
            || stats.last_sdk_result != FV1_SDK_OK) {
            fprintf(
                stderr,
                "FAIL  %-20s  stats chip=%llu last=%d\n",
                demo->label,
                (unsigned long long)stats.chip_frames,
                (int)stats.last_sdk_result);
            fv1_apple_realtime_destroy(bridge);
            return 1;
        }
    }

    printf(
        "PASS  %-20s  instructions=%u delay_high=%u\n",
        demo->label,
        report.instruction_count,
        report.highest_delay_address);

    fv1_apple_realtime_destroy(bridge);
    return 0;
}

int main(int argc, char** argv) {
    const char* root;
    size_t i;
    int failures = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <repository-root>\n", argv[0]);
        return 2;
    }

    root = argv[1];

    puts("FV-1 Lab macOS Phase 8D shipped-program regression");
    puts("---------------------------------------------------");

    for (i = 0u;
         i < sizeof(k_demos) / sizeof(k_demos[0]);
         ++i) {
        failures += exercise_program(root, &k_demos[i]);
    }

    if (failures != 0) {
        fprintf(stderr, "%d shipped program(s) failed\n", failures);
        return 1;
    }

    puts("All shipped SpinASM programs compiled, loaded, and executed.");
    return 0;
}
