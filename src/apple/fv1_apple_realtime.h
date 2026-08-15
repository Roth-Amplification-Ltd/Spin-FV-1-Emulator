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

#ifdef __cplusplus
}
#endif

#endif
