#include "fv1_apple_realtime.h"

#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define FV1_APPLE_VIRTUAL_SAMPLE_RATE 32768.0
#define FV1_APPLE_DEFAULT_FIFO_FRAMES 32768u
#define FV1_APPLE_SCOPE_FRAMES 2048u
#define FV1_APPLE_RESAMPLER_EPSILON 1.0e-12

typedef struct stereo_ring {
    float* samples;
    uint32_t capacity_frames;
    atomic_uint_least64_t read_index;
    atomic_uint_least64_t write_index;
} stereo_ring;

typedef struct linear_resampler {
    double source_rate;
    double target_rate;
    double step;
    double next_output_position;
    uint64_t input_index;
    float previous_left;
    float previous_right;
    int have_previous;
} linear_resampler;

struct fv1_apple_realtime {
    fv1_sdk_engine* engine;
    stereo_ring output_ring;
    linear_resampler input_to_chip;
    linear_resampler chip_to_output;

    atomic_uint_least32_t desired_pot_bits[3];
    uint32_t applied_pot_bits[3];

    atomic_uint_least64_t input_frames;
    atomic_uint_least64_t chip_frames;
    atomic_uint_least64_t generated_output_frames;
    atomic_uint_least64_t rendered_output_frames;
    atomic_uint_least64_t output_underflows;
    atomic_uint_least64_t output_overflows;
    atomic_int_least32_t last_sdk_result;

    float scope[FV1_APPLE_SCOPE_FRAMES * 2u];
    atomic_uint_least64_t scope_write_index;
};

static uint32_t float_to_bits(float value) {
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float bits_to_float(uint32_t bits) {
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float clamp_pot(float value) {
    if (!isfinite(value)) return 0.0f;
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static int valid_rate(double rate) {
    return isfinite(rate) && rate >= 8000.0 && rate <= 384000.0;
}

static void resampler_reset(linear_resampler* resampler,
                            double source_rate,
                            double target_rate) {
    if (!resampler) return;
    memset(resampler, 0, sizeof(*resampler));
    resampler->source_rate = source_rate;
    resampler->target_rate = target_rate;
    resampler->step = source_rate / target_rate;
}

static int ring_init(stereo_ring* ring, uint32_t capacity_frames) {
    if (!ring || capacity_frames < 64u) return 0;
    memset(ring, 0, sizeof(*ring));
    ring->samples = (float*)calloc((size_t)capacity_frames * 2u, sizeof(float));
    if (!ring->samples) return 0;
    ring->capacity_frames = capacity_frames;
    atomic_init(&ring->read_index, 0u);
    atomic_init(&ring->write_index, 0u);
    return 1;
}

static void ring_destroy(stereo_ring* ring) {
    if (!ring) return;
    free(ring->samples);
    ring->samples = NULL;
    ring->capacity_frames = 0u;
}

static void ring_clear(stereo_ring* ring) {
    if (!ring) return;
    atomic_store_explicit(&ring->read_index, 0u, memory_order_release);
    atomic_store_explicit(&ring->write_index, 0u, memory_order_release);
}

static int ring_push(stereo_ring* ring, float left, float right) {
    const uint64_t write_index = atomic_load_explicit(&ring->write_index, memory_order_relaxed);
    const uint64_t read_index = atomic_load_explicit(&ring->read_index, memory_order_acquire);
    if ((write_index - read_index) >= (uint64_t)ring->capacity_frames) return 0;

    const size_t slot = (size_t)(write_index % (uint64_t)ring->capacity_frames) * 2u;
    ring->samples[slot] = left;
    ring->samples[slot + 1u] = right;
    atomic_store_explicit(&ring->write_index, write_index + 1u, memory_order_release);
    return 1;
}

static int ring_pop(stereo_ring* ring, float* left, float* right) {
    const uint64_t read_index = atomic_load_explicit(&ring->read_index, memory_order_relaxed);
    const uint64_t write_index = atomic_load_explicit(&ring->write_index, memory_order_acquire);
    if (read_index == write_index) return 0;

    const size_t slot = (size_t)(read_index % (uint64_t)ring->capacity_frames) * 2u;
    *left = ring->samples[slot];
    *right = ring->samples[slot + 1u];
    atomic_store_explicit(&ring->read_index, read_index + 1u, memory_order_release);
    return 1;
}

static void write_scope(fv1_apple_realtime* bridge, float left, float right) {
    const uint64_t index = atomic_load_explicit(&bridge->scope_write_index, memory_order_relaxed);
    const size_t slot = (size_t)(index % FV1_APPLE_SCOPE_FRAMES) * 2u;
    bridge->scope[slot] = left;
    bridge->scope[slot + 1u] = right;
    atomic_store_explicit(&bridge->scope_write_index, index + 1u, memory_order_release);
}

static void push_output_sample(fv1_apple_realtime* bridge, float left, float right) {
    if (!ring_push(&bridge->output_ring, left, right)) {
        atomic_fetch_add_explicit(&bridge->output_overflows, 1u, memory_order_relaxed);
        return;
    }
    atomic_fetch_add_explicit(&bridge->generated_output_frames, 1u, memory_order_relaxed);
    write_scope(bridge, left, right);
}

static void feed_output_resampler(fv1_apple_realtime* bridge, float left, float right) {
    linear_resampler* resampler = &bridge->chip_to_output;
    if (!resampler->have_previous) {
        resampler->previous_left = left;
        resampler->previous_right = right;
        resampler->input_index = 0u;
        resampler->next_output_position = resampler->step;
        resampler->have_previous = 1;
        push_output_sample(bridge, left, right);
        return;
    }

    const uint64_t current_index = resampler->input_index + 1u;
    while (resampler->next_output_position <= (double)current_index + FV1_APPLE_RESAMPLER_EPSILON) {
        double alpha = resampler->next_output_position - (double)(current_index - 1u);
        if (alpha < 0.0) alpha = 0.0;
        if (alpha > 1.0) alpha = 1.0;
        const float out_left = resampler->previous_left + (left - resampler->previous_left) * (float)alpha;
        const float out_right = resampler->previous_right + (right - resampler->previous_right) * (float)alpha;
        push_output_sample(bridge, out_left, out_right);
        resampler->next_output_position += resampler->step;
    }

    resampler->previous_left = left;
    resampler->previous_right = right;
    resampler->input_index = current_index;
}

static void apply_pending_pots(fv1_apple_realtime* bridge) {
    uint32_t bits[3];
    int changed = 0;
    for (uint32_t i = 0u; i < 3u; ++i) {
        bits[i] = (uint32_t)atomic_load_explicit(&bridge->desired_pot_bits[i], memory_order_acquire);
        if (bits[i] != bridge->applied_pot_bits[i]) changed = 1;
    }
    if (!changed) return;

    const fv1_sdk_result result = fv1_sdk_engine_set_pots(bridge->engine,
                                                           bits_to_float(bits[0]),
                                                           bits_to_float(bits[1]),
                                                           bits_to_float(bits[2]));
    atomic_store_explicit(&bridge->last_sdk_result, result, memory_order_relaxed);
    if (result == FV1_SDK_OK) {
        bridge->applied_pot_bits[0] = bits[0];
        bridge->applied_pot_bits[1] = bits[1];
        bridge->applied_pot_bits[2] = bits[2];
    }
}

static fv1_sdk_result process_chip_sample(fv1_apple_realtime* bridge,
                                           float input_left,
                                           float input_right) {
    float output_left = 0.0f;
    float output_right = 0.0f;
    const fv1_sdk_result result = fv1_sdk_engine_process_sample_f32(bridge->engine,
                                                                    input_left,
                                                                    input_right,
                                                                    &output_left,
                                                                    &output_right);
    atomic_store_explicit(&bridge->last_sdk_result, result, memory_order_relaxed);
    if (result != FV1_SDK_OK) return result;

    atomic_fetch_add_explicit(&bridge->chip_frames, 1u, memory_order_relaxed);
    feed_output_resampler(bridge, output_left, output_right);
    return FV1_SDK_OK;
}

static fv1_sdk_result feed_input_resampler(fv1_apple_realtime* bridge,
                                            float left,
                                            float right) {
    linear_resampler* resampler = &bridge->input_to_chip;
    if (!resampler->have_previous) {
        resampler->previous_left = left;
        resampler->previous_right = right;
        resampler->input_index = 0u;
        resampler->next_output_position = resampler->step;
        resampler->have_previous = 1;
        return process_chip_sample(bridge, left, right);
    }

    const uint64_t current_index = resampler->input_index + 1u;
    while (resampler->next_output_position <= (double)current_index + FV1_APPLE_RESAMPLER_EPSILON) {
        double alpha = resampler->next_output_position - (double)(current_index - 1u);
        if (alpha < 0.0) alpha = 0.0;
        if (alpha > 1.0) alpha = 1.0;
        const float chip_left = resampler->previous_left + (left - resampler->previous_left) * (float)alpha;
        const float chip_right = resampler->previous_right + (right - resampler->previous_right) * (float)alpha;
        const fv1_sdk_result result = process_chip_sample(bridge, chip_left, chip_right);
        if (result != FV1_SDK_OK) return result;
        resampler->next_output_position += resampler->step;
    }

    resampler->previous_left = left;
    resampler->previous_right = right;
    resampler->input_index = current_index;
    return FV1_SDK_OK;
}

static fv1_sdk_result load_default_passthrough(fv1_apple_realtime* bridge) {
    static const char source[] =
        "RDAX ADCL, 1.0\n"
        "WRAX DACL, 0\n"
        "RDAX ADCR, 1.0\n"
        "WRAX DACR, 0\n";
    uint8_t program[FV1_SDK_PROGRAM_BYTES];
    fv1_sdk_compile_report_v1 report;
    char diagnostic[256];
    fv1_sdk_compile_report_v1_init(&report);
    memset(program, 0, sizeof(program));
    memset(diagnostic, 0, sizeof(diagnostic));
    const fv1_sdk_result compile_result = fv1_sdk_compile_spinasm_v1(source,
                                                                     sizeof(source) - 1u,
                                                                     program,
                                                                     sizeof(program),
                                                                     &report,
                                                                     diagnostic,
                                                                     sizeof(diagnostic));
    if (compile_result != FV1_SDK_OK) return compile_result;
    return fv1_sdk_engine_load_program(bridge->engine, program, sizeof(program));
}

void fv1_apple_realtime_stats_v1_init(fv1_apple_realtime_stats_v1* stats) {
    if (!stats) return;
    memset(stats, 0, sizeof(*stats));
    stats->struct_size = (uint32_t)sizeof(*stats);
    stats->last_sdk_result = FV1_SDK_OK;
}

fv1_sdk_result fv1_apple_realtime_create(double input_sample_rate,
                                          double output_sample_rate,
                                          uint32_t output_capacity_frames,
                                          fv1_apple_realtime** out_bridge) {
    if (!out_bridge || !valid_rate(input_sample_rate) || !valid_rate(output_sample_rate)) {
        return FV1_SDK_ERROR_INVALID_ARGUMENT;
    }
    *out_bridge = NULL;

    fv1_apple_realtime* bridge = (fv1_apple_realtime*)calloc(1u, sizeof(*bridge));
    if (!bridge) return FV1_SDK_ERROR_OUT_OF_MEMORY;

    if (output_capacity_frames == 0u) output_capacity_frames = FV1_APPLE_DEFAULT_FIFO_FRAMES;
    if (!ring_init(&bridge->output_ring, output_capacity_frames)) {
        free(bridge);
        return FV1_SDK_ERROR_OUT_OF_MEMORY;
    }

    fv1_sdk_engine_config_v1 config;
    fv1_sdk_engine_config_v1_init(&config);
    config.virtual_sample_rate = FV1_APPLE_VIRTUAL_SAMPLE_RATE;
    fv1_sdk_result result = fv1_sdk_engine_create_v1(&config, &bridge->engine);
    if (result != FV1_SDK_OK) {
        ring_destroy(&bridge->output_ring);
        free(bridge);
        return result;
    }

    result = load_default_passthrough(bridge);
    if (result != FV1_SDK_OK) {
        fv1_sdk_engine_destroy(bridge->engine);
        ring_destroy(&bridge->output_ring);
        free(bridge);
        return result;
    }

    resampler_reset(&bridge->input_to_chip, input_sample_rate, FV1_APPLE_VIRTUAL_SAMPLE_RATE);
    resampler_reset(&bridge->chip_to_output, FV1_APPLE_VIRTUAL_SAMPLE_RATE, output_sample_rate);

    for (uint32_t i = 0u; i < 3u; ++i) {
        const uint32_t bits = float_to_bits(0.0f);
        atomic_init(&bridge->desired_pot_bits[i], bits);
        bridge->applied_pot_bits[i] = bits;
    }
    atomic_init(&bridge->input_frames, 0u);
    atomic_init(&bridge->chip_frames, 0u);
    atomic_init(&bridge->generated_output_frames, 0u);
    atomic_init(&bridge->rendered_output_frames, 0u);
    atomic_init(&bridge->output_underflows, 0u);
    atomic_init(&bridge->output_overflows, 0u);
    atomic_init(&bridge->last_sdk_result, FV1_SDK_OK);
    atomic_init(&bridge->scope_write_index, 0u);

    *out_bridge = bridge;
    return FV1_SDK_OK;
}

void fv1_apple_realtime_destroy(fv1_apple_realtime* bridge) {
    if (!bridge) return;
    fv1_sdk_engine_destroy(bridge->engine);
    bridge->engine = NULL;
    ring_destroy(&bridge->output_ring);
    free(bridge);
}

fv1_sdk_result fv1_apple_realtime_configure_rates(fv1_apple_realtime* bridge,
                                                   double input_sample_rate,
                                                   double output_sample_rate) {
    if (!bridge || !valid_rate(input_sample_rate) || !valid_rate(output_sample_rate)) {
        return FV1_SDK_ERROR_INVALID_ARGUMENT;
    }
    resampler_reset(&bridge->input_to_chip, input_sample_rate, FV1_APPLE_VIRTUAL_SAMPLE_RATE);
    resampler_reset(&bridge->chip_to_output, FV1_APPLE_VIRTUAL_SAMPLE_RATE, output_sample_rate);
    ring_clear(&bridge->output_ring);
    return FV1_SDK_OK;
}

fv1_sdk_result fv1_apple_realtime_load_program(fv1_apple_realtime* bridge,
                                                const uint8_t* program,
                                                size_t program_size) {
    if (!bridge) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    const fv1_sdk_result result = fv1_sdk_engine_load_program(bridge->engine, program, program_size);
    atomic_store_explicit(&bridge->last_sdk_result, result, memory_order_relaxed);
    if (result == FV1_SDK_OK) {
        resampler_reset(&bridge->input_to_chip, bridge->input_to_chip.source_rate, bridge->input_to_chip.target_rate);
        resampler_reset(&bridge->chip_to_output, bridge->chip_to_output.source_rate, bridge->chip_to_output.target_rate);
        ring_clear(&bridge->output_ring);
    }
    return result;
}

fv1_sdk_result fv1_apple_realtime_reset(fv1_apple_realtime* bridge,
                                         uint32_t clear_delay_ram) {
    if (!bridge) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    const fv1_sdk_result result = fv1_sdk_engine_reset(bridge->engine, clear_delay_ram);
    atomic_store_explicit(&bridge->last_sdk_result, result, memory_order_relaxed);
    if (result == FV1_SDK_OK) {
        resampler_reset(&bridge->input_to_chip, bridge->input_to_chip.source_rate, bridge->input_to_chip.target_rate);
        resampler_reset(&bridge->chip_to_output, bridge->chip_to_output.source_rate, bridge->chip_to_output.target_rate);
        ring_clear(&bridge->output_ring);
    }
    return result;
}

void fv1_apple_realtime_flush(fv1_apple_realtime* bridge) {
    if (!bridge) return;
    resampler_reset(&bridge->input_to_chip, bridge->input_to_chip.source_rate, bridge->input_to_chip.target_rate);
    resampler_reset(&bridge->chip_to_output, bridge->chip_to_output.source_rate, bridge->chip_to_output.target_rate);
    ring_clear(&bridge->output_ring);
    atomic_store_explicit(&bridge->scope_write_index, 0u, memory_order_release);
    memset(bridge->scope, 0, sizeof(bridge->scope));
}

void fv1_apple_realtime_prime_silence(fv1_apple_realtime* bridge,
                                       uint32_t frames) {
    if (!bridge) return;
    if (frames > bridge->output_ring.capacity_frames / 2u) {
        frames = bridge->output_ring.capacity_frames / 2u;
    }
    for (uint32_t i = 0u; i < frames; ++i) {
        if (!ring_push(&bridge->output_ring, 0.0f, 0.0f)) break;
    }
}

void fv1_apple_realtime_set_pots(fv1_apple_realtime* bridge,
                                  float pot0,
                                  float pot1,
                                  float pot2) {
    if (!bridge) return;
    const float values[3] = {clamp_pot(pot0), clamp_pot(pot1), clamp_pot(pot2)};
    for (uint32_t i = 0u; i < 3u; ++i) {
        atomic_store_explicit(&bridge->desired_pot_bits[i], float_to_bits(values[i]), memory_order_release);
    }
}

fv1_sdk_result fv1_apple_realtime_process_planar_input(fv1_apple_realtime* bridge,
                                                        const float* input_left,
                                                        const float* input_right,
                                                        size_t frames) {
    if (!bridge || (frames > 0u && (!input_left || !input_right))) {
        return FV1_SDK_ERROR_INVALID_ARGUMENT;
    }
    if (frames == 0u) return FV1_SDK_OK;

    apply_pending_pots(bridge);
    for (size_t i = 0u; i < frames; ++i) {
        const fv1_sdk_result result = feed_input_resampler(bridge, input_left[i], input_right[i]);
        if (result != FV1_SDK_OK) return result;
    }
    atomic_fetch_add_explicit(&bridge->input_frames, (uint64_t)frames, memory_order_relaxed);
    return FV1_SDK_OK;
}

void fv1_apple_realtime_render_planar_output(fv1_apple_realtime* bridge,
                                              float* output_left,
                                              float* output_right,
                                              size_t frames) {
    if (!bridge || !output_left || !output_right) return;
    for (size_t i = 0u; i < frames; ++i) {
        float left = 0.0f;
        float right = 0.0f;
        if (!ring_pop(&bridge->output_ring, &left, &right)) {
            atomic_fetch_add_explicit(&bridge->output_underflows, 1u, memory_order_relaxed);
        }
        output_left[i] = left;
        output_right[i] = right;
    }
    atomic_fetch_add_explicit(&bridge->rendered_output_frames, (uint64_t)frames, memory_order_relaxed);
}

void fv1_apple_realtime_get_stats(const fv1_apple_realtime* bridge,
                                   fv1_apple_realtime_stats_v1* stats) {
    if (!bridge || !stats) return;
    fv1_apple_realtime_stats_v1_init(stats);
    stats->input_frames = atomic_load_explicit(&bridge->input_frames, memory_order_acquire);
    stats->chip_frames = atomic_load_explicit(&bridge->chip_frames, memory_order_acquire);
    stats->generated_output_frames = atomic_load_explicit(&bridge->generated_output_frames, memory_order_acquire);
    stats->rendered_output_frames = atomic_load_explicit(&bridge->rendered_output_frames, memory_order_acquire);
    stats->output_underflows = atomic_load_explicit(&bridge->output_underflows, memory_order_acquire);
    stats->output_overflows = atomic_load_explicit(&bridge->output_overflows, memory_order_acquire);
    stats->last_sdk_result = atomic_load_explicit(&bridge->last_sdk_result, memory_order_acquire);
}

size_t fv1_apple_realtime_copy_scope(const fv1_apple_realtime* bridge,
                                      float* output_left,
                                      float* output_right,
                                      size_t capacity_frames) {
    if (!bridge || !output_left || !output_right || capacity_frames == 0u) return 0u;
    uint64_t write_index = atomic_load_explicit(&bridge->scope_write_index, memory_order_acquire);
    size_t available = write_index < FV1_APPLE_SCOPE_FRAMES ? (size_t)write_index : FV1_APPLE_SCOPE_FRAMES;
    if (available > capacity_frames) available = capacity_frames;
    const uint64_t start = write_index - (uint64_t)available;
    for (size_t i = 0u; i < available; ++i) {
        const size_t slot = (size_t)((start + (uint64_t)i) % FV1_APPLE_SCOPE_FRAMES) * 2u;
        output_left[i] = bridge->scope[slot];
        output_right[i] = bridge->scope[slot + 1u];
    }
    return available;
}
