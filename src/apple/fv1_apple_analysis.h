#ifndef FV1_APPLE_ANALYSIS_H
#define FV1_APPLE_ANALYSIS_H

#include <stddef.h>
#include <stdint.h>
#include "fv1_apple_realtime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fv1_apple_analysis_state fv1_apple_analysis_state;

fv1_sdk_result fv1_apple_analysis_create(
    double raw_sample_rate,
    double processed_sample_rate,
    size_t fft_size,
    fv1_apple_analysis_state** out_state);

void fv1_apple_analysis_destroy(fv1_apple_analysis_state* state);

fv1_sdk_result fv1_apple_analysis_configure(
    fv1_apple_analysis_state* state,
    double raw_sample_rate,
    double processed_sample_rate,
    size_t fft_size);

void fv1_apple_analysis_push_raw_planar(
    fv1_apple_analysis_state* state,
    const float* left,
    const float* right,
    size_t frames);

void fv1_apple_analysis_push_raw_sample(
    fv1_apple_analysis_state* state,
    float left,
    float right);

void fv1_apple_analysis_push_processed_sample(
    fv1_apple_analysis_state* state,
    float left,
    float right);

fv1_sdk_result fv1_apple_analysis_copy(
    const fv1_apple_analysis_state* state,
    uint32_t stream,
    fv1_apple_analysis_snapshot_v1* snapshot,
    float* spectrum_db,
    size_t spectrum_capacity,
    float* scope_left,
    float* scope_right,
    size_t scope_capacity);

fv1_sdk_result fv1_apple_analysis_start_recording(
    fv1_apple_analysis_state* state,
    const char* path,
    uint32_t sample_rate,
    uint32_t mode,
    char* error,
    size_t error_capacity);

void fv1_apple_analysis_stop_recording(
    fv1_apple_analysis_state* state);

uint32_t fv1_apple_analysis_is_recording(
    const fv1_apple_analysis_state* state);

void fv1_apple_analysis_get_recorder_stats(
    const fv1_apple_analysis_state* state,
    fv1_apple_recorder_stats_v1* stats);

#ifdef __cplusplus
}
#endif
#endif
