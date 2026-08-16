#ifndef FV1_APPLE_REALTIME_H
#define FV1_APPLE_REALTIME_H

#include <stddef.h>
#include <stdint.h>

#include <fv1/sdk.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform-neutral realtime bridge used by the native Apple frontend.
 *
 * The bridge deliberately consumes only the public FV-1 SDK.  It owns a
 * dedicated realtime SDK engine, a fixed-capacity stereo output FIFO, two
 * allocation-free linear clock-domain converters, atomic POT handoff, and a
 * small oscilloscope history buffer.  Creation/configuration/program loading
 * happen off the audio render threads; process/render calls do not allocate,
 * lock, access files, or log.
 */

typedef struct fv1_apple_realtime fv1_apple_realtime;

/* Values intentionally match the Linux TestSignalKind ordering. */
enum {
    FV1_APPLE_TEST_SIGNAL_SINE = 0u,
    FV1_APPLE_TEST_SIGNAL_SWEEP = 1u,
    FV1_APPLE_TEST_SIGNAL_WHITE_NOISE = 2u,
    FV1_APPLE_TEST_SIGNAL_PINK_NOISE = 3u,
    FV1_APPLE_TEST_SIGNAL_IMPULSE = 4u
};

typedef struct fv1_apple_realtime_stats_v1 {
    uint32_t struct_size;
    uint32_t reserved0;
    uint64_t input_frames;
    uint64_t chip_frames;
    uint64_t generated_output_frames;
    uint64_t rendered_output_frames;
    uint64_t output_underflows;
    uint64_t output_overflows;
    int32_t last_sdk_result;
    uint32_t reserved[7];
} fv1_apple_realtime_stats_v1;

void fv1_apple_realtime_stats_v1_init(fv1_apple_realtime_stats_v1* stats);

enum {
    FV1_APPLE_ANALYSIS_RAW = 0u,
    FV1_APPLE_ANALYSIS_PROCESSED = 1u
};

enum {
    FV1_APPLE_RECORD_PROCESSED = 0u,
    FV1_APPLE_RECORD_RAW = 1u,
    FV1_APPLE_RECORD_RAW_AND_PROCESSED = 2u
};

typedef struct fv1_apple_analysis_snapshot_v1 {
    uint32_t struct_size;
    uint32_t spectrum_bins;
    uint32_t scope_frames;
    uint32_t reserved0;
    double sample_rate;
    uint64_t sequence;
    uint64_t dropped_frames;
    float peak_left;
    float peak_right;
    float rms_left;
    float rms_right;
    float correlation;
    float dominant_frequency_hz;
    float dominant_level_db;
    uint32_t reserved[8];
} fv1_apple_analysis_snapshot_v1;

typedef struct fv1_apple_recorder_stats_v1 {
    uint32_t struct_size;
    uint32_t reserved0;
    uint64_t raw_frames_written;
    uint64_t processed_frames_written;
    uint64_t raw_frames_dropped;
    uint64_t processed_frames_dropped;
    uint32_t reserved[8];
} fv1_apple_recorder_stats_v1;

void fv1_apple_analysis_snapshot_v1_init(
    fv1_apple_analysis_snapshot_v1* snapshot);
void fv1_apple_recorder_stats_v1_init(
    fv1_apple_recorder_stats_v1* stats);

fv1_sdk_result fv1_apple_realtime_create(double input_sample_rate,
                                          double output_sample_rate,
                                          uint32_t output_capacity_frames,
                                          fv1_apple_realtime** out_bridge);
void fv1_apple_realtime_destroy(fv1_apple_realtime* bridge);

/* Call only while the Apple audio engine is stopped. */
fv1_sdk_result fv1_apple_realtime_configure_rates(fv1_apple_realtime* bridge,
                                                   double input_sample_rate,
                                                   double output_sample_rate);
fv1_sdk_result fv1_apple_realtime_load_program(fv1_apple_realtime* bridge,
                                                const uint8_t* program,
                                                size_t program_size);
fv1_sdk_result fv1_apple_realtime_reset(fv1_apple_realtime* bridge,
                                         uint32_t clear_delay_ram);
void fv1_apple_realtime_flush(fv1_apple_realtime* bridge);
void fv1_apple_realtime_prime_silence(fv1_apple_realtime* bridge,
                                       uint32_t frames);

/*
 * POT writes are atomic and may originate on the main/UI thread.  The values
 * are applied to the SDK engine by the input/audio callback between process
 * calls, so the SDK engine itself remains single-thread owned.
 */
void fv1_apple_realtime_set_pots(fv1_apple_realtime* bridge,
                                  float pot0,
                                  float pot1,
                                  float pot2);

/*
 * Atomic realtime DSP routing. Bypass changes only the audible route; the
 * virtual FV-1 continues running so RAW and PROCESSED analysis stays live.
 */
void fv1_apple_realtime_set_dsp_enabled(
    fv1_apple_realtime* bridge,
    uint32_t enabled);
uint32_t fv1_apple_realtime_get_dsp_enabled(
    const fv1_apple_realtime* bridge);

/*
 * Built-in test generator.  Configuration writes are atomic and may come from
 * the main/UI thread while audio is running.  Generation itself occurs only
 * from the audio callback and does not allocate, lock, access files, or log.
 *
 * Defaults mirror the Linux FV-1 Lab test generator:
 *   Sine, 440 Hz, amplitude 0.25, sweep end 12 kHz / 5 s,
 *   impulse period 1 s, deterministic noise seed 0x465631.
 */
void fv1_apple_realtime_set_test_generator(fv1_apple_realtime* bridge,
                                            uint32_t kind,
                                            double frequency_hz,
                                            double amplitude,
                                            double sweep_end_hz,
                                            double sweep_seconds,
                                            double impulse_period_seconds);

fv1_sdk_result fv1_apple_realtime_process_test_generator(
    fv1_apple_realtime* bridge,
    size_t frames);

/*
 * Input is non-interleaved Float32 as provided by AVAudioPCMBuffer.  Mono input
 * may pass the same pointer for left/right.  The callback may use any source
 * sample rate configured above; samples are converted to the FV-1's 32.768 kHz
 * virtual domain before SDK processing.
 */
fv1_sdk_result fv1_apple_realtime_process_planar_input(fv1_apple_realtime* bridge,
                                                        const float* input_left,
                                                        const float* input_right,
                                                        size_t frames);

/*
 * Output is non-interleaved Float32.  Missing frames are zero-filled and count
 * as underflows rather than blocking the render callback.
 */
void fv1_apple_realtime_render_planar_output(fv1_apple_realtime* bridge,
                                              float* output_left,
                                              float* output_right,
                                              size_t frames);

void fv1_apple_realtime_get_stats(const fv1_apple_realtime* bridge,
                                   fv1_apple_realtime_stats_v1* stats);

/* Copies newest-first history into oldest-to-newest planar buffers. */
size_t fv1_apple_realtime_copy_scope(const fv1_apple_realtime* bridge,
                                      float* output_left,
                                      float* output_right,
                                      size_t capacity_frames);

/* Shared Linux/Apple analyzer snapshot surface. */
fv1_sdk_result fv1_apple_realtime_copy_analysis(
    const fv1_apple_realtime* bridge,
    uint32_t stream,
    fv1_apple_analysis_snapshot_v1* snapshot,
    float* spectrum_db,
    size_t spectrum_capacity,
    float* scope_left,
    float* scope_right,
    size_t scope_capacity);

/* Shared realtime-safe WAV recorder surface. */
fv1_sdk_result fv1_apple_realtime_start_recording(
    fv1_apple_realtime* bridge,
    const char* path,
    uint32_t sample_rate,
    uint32_t mode,
    char* error,
    size_t error_capacity);
void fv1_apple_realtime_stop_recording(
    fv1_apple_realtime* bridge);
uint32_t fv1_apple_realtime_is_recording(
    const fv1_apple_realtime* bridge);
void fv1_apple_realtime_get_recorder_stats(
    const fv1_apple_realtime* bridge,
    fv1_apple_recorder_stats_v1* stats);

#ifdef __cplusplus
}
#endif

#endif
