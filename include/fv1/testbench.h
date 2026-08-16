#ifndef FV1_TESTBENCH_H
#define FV1_TESTBENCH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform-neutral C façade for reusable FV-1 Lab/testbench services that are
 * intentionally outside the small embeddable FV-1 engine SDK ABI.
 *
 * The implementation wraps the same FileLoopSource and validation framework
 * used by the Linux FV-1 Lab. Native frontends can therefore share behavior
 * without importing private C++ headers or duplicating algorithms.
 */

typedef int32_t fv1_testbench_result;
enum {
    FV1_TESTBENCH_OK = 0,
    FV1_TESTBENCH_ERROR_INVALID_ARGUMENT = -1,
    FV1_TESTBENCH_ERROR_BAD_STATE = -2,
    FV1_TESTBENCH_ERROR_IO = -3,
    FV1_TESTBENCH_ERROR_UNSUPPORTED = -4,
    FV1_TESTBENCH_ERROR_OUT_OF_MEMORY = -5,
    FV1_TESTBENCH_ERROR_INTERNAL = -6
};

typedef struct fv1_testbench_file_source fv1_testbench_file_source;

enum {
    FV1_TESTBENCH_TRANSPORT_STOPPED = 0u,
    FV1_TESTBENCH_TRANSPORT_PLAYING = 1u,
    FV1_TESTBENCH_TRANSPORT_PAUSED = 2u
};

typedef struct fv1_testbench_file_info_v1 {
    uint32_t struct_size;
    uint32_t transport_state;
    uint32_t looping;
    uint32_t file_sample_rate;
    uint64_t total_frames;
    double duration_seconds;
    double position_seconds;
    double loop_begin_seconds;
    double loop_end_seconds;
    double loop_crossfade_ms;
    uint32_t reserved[8];
} fv1_testbench_file_info_v1;

void fv1_testbench_file_info_v1_init(
    fv1_testbench_file_info_v1* info);

fv1_testbench_result fv1_testbench_file_source_create(
    fv1_testbench_file_source** out_source);

void fv1_testbench_file_source_destroy(
    fv1_testbench_file_source* source);

fv1_testbench_result fv1_testbench_file_source_load(
    fv1_testbench_file_source* source,
    const char* path_utf8,
    char* error_utf8,
    size_t error_capacity);

fv1_testbench_result fv1_testbench_file_source_prepare(
    fv1_testbench_file_source* source,
    double host_sample_rate,
    size_t max_block_frames);

/*
 * Realtime render boundary. After load()/prepare(), this function performs no
 * filesystem I/O and delegates to FileLoopSource's allocation-free renderer.
 * Output is non-interleaved stereo Float32.
 */
fv1_testbench_result fv1_testbench_file_source_render_planar(
    fv1_testbench_file_source* source,
    float* output_left,
    float* output_right,
    size_t frames);

void fv1_testbench_file_source_play(
    fv1_testbench_file_source* source);
void fv1_testbench_file_source_pause(
    fv1_testbench_file_source* source);
void fv1_testbench_file_source_stop(
    fv1_testbench_file_source* source);

void fv1_testbench_file_source_set_looping(
    fv1_testbench_file_source* source,
    uint32_t enabled);

fv1_testbench_result fv1_testbench_file_source_seek_seconds(
    fv1_testbench_file_source* source,
    double seconds);

fv1_testbench_result fv1_testbench_file_source_set_loop_region_seconds(
    fv1_testbench_file_source* source,
    double begin_seconds,
    double end_seconds);

void fv1_testbench_file_source_set_crossfade_ms(
    fv1_testbench_file_source* source,
    double milliseconds);

fv1_testbench_result fv1_testbench_file_source_get_info(
    const fv1_testbench_file_source* source,
    fv1_testbench_file_info_v1* info);


typedef struct fv1_testbench_validation_config_v1 {
    uint32_t struct_size;
    uint32_t gain_match_residual;
    uint32_t fft_size;
    uint32_t reserved0;
    double max_alignment_ms;
    double spectral_floor_db;
    double minimum_correlation;
    double maximum_residual_rms_dbfs;
    double maximum_residual_peak_dbfs;
    uint32_t reserved[8];
} fv1_testbench_validation_config_v1;

typedef struct fv1_testbench_validation_summary_v1 {
    uint32_t struct_size;
    uint32_t passed;
    uint32_t sample_rate;
    uint32_t failure_count;
    int64_t capture_delay_frames;
    uint64_t compared_frames;
    double capture_delay_ms;
    double applied_capture_gain_db;

    double left_reference_rms_dbfs;
    double left_capture_rms_dbfs;
    double left_gain_error_db;
    double left_correlation;
    double left_residual_rms_dbfs;
    double left_residual_peak_dbfs;
    double left_snr_db;

    double right_reference_rms_dbfs;
    double right_capture_rms_dbfs;
    double right_gain_error_db;
    double right_correlation;
    double right_residual_rms_dbfs;
    double right_residual_peak_dbfs;
    double right_snr_db;

    double spectral_rms_magnitude_error_db;
    double spectral_worst_magnitude_error_db;
    double spectral_worst_phase_error_degrees;
    uint32_t reserved[8];
} fv1_testbench_validation_summary_v1;

void fv1_testbench_validation_config_v1_init(
    fv1_testbench_validation_config_v1* config);
void fv1_testbench_validation_summary_v1_init(
    fv1_testbench_validation_summary_v1* summary);

/*
 * Compare two WAVs using the shared Phase-5 validation engine.
 *
 * report_prefix_utf8 may be NULL/empty. If supplied, the standard .json,
 * .md, frequency CSV and residual WAV report bundle is written there.
 *
 * A completed comparison that fails thresholds still returns
 * FV1_TESTBENCH_OK; inspect summary->passed and failure_text_utf8.
 */
fv1_testbench_result fv1_testbench_validate_wavs(
    const char* reference_path_utf8,
    const char* capture_path_utf8,
    const fv1_testbench_validation_config_v1* config,
    const char* report_prefix_utf8,
    fv1_testbench_validation_summary_v1* summary,
    char* failure_text_utf8,
    size_t failure_text_capacity,
    char* error_utf8,
    size_t error_capacity);

/*
 * Generate the deterministic hardware-validation stimulus directory used by
 * the Linux testbench: impulse, multitone, sweep, sine, white and pink stimuli
 * plus manifest/fixture documentation.
 */
fv1_testbench_result fv1_testbench_write_validation_pack(
    const char* directory_utf8,
    uint32_t sample_rate,
    double seconds,
    double level,
    uint32_t seed,
    char* error_utf8,
    size_t error_capacity);

#ifdef __cplusplus
}
#endif

#endif /* FV1_TESTBENCH_H */
