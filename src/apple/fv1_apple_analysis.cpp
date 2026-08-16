#include "fv1_apple_analysis.h"

#include <fv1/analysis.hpp>
#include <fv1/audio_recorder.hpp>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <new>
#include <string>

namespace {
constexpr std::size_t kPushChunk = 256u;

void copy_error(char* output, std::size_t capacity, const std::string& message) {
    if (!output || capacity == 0u) return;
    std::snprintf(output, capacity, "%s", message.c_str());
}
}

struct fv1_apple_analysis_state {
    fv1::AnalyzerWorker raw;
    fv1::AnalyzerWorker processed;
    fv1::AudioRecorder recorder;
    std::atomic<bool> recorder_enabled{false};
    double raw_rate{48000.0};
    double processed_rate{48000.0};
    std::size_t fft_size{4096u};
};

namespace {
const fv1::AnalyzerWorker* analyzer_for(const fv1_apple_analysis_state* state, std::uint32_t stream) {
    if (!state) return nullptr;
    if (stream == FV1_APPLE_ANALYSIS_RAW) return &state->raw;
    if (stream == FV1_APPLE_ANALYSIS_PROCESSED) return &state->processed;
    return nullptr;
}

bool prepare_workers(
    fv1_apple_analysis_state* state,
    double raw_rate,
    double processed_rate,
    std::size_t fft_size) {

    const std::size_t queue_frames = std::max<std::size_t>(65536u, fft_size * 16u);

    if (!state->raw.prepare(raw_rate, fft_size, queue_frames)) return false;
    if (!state->processed.prepare(processed_rate, fft_size, queue_frames)) return false;

    state->raw_rate = raw_rate;
    state->processed_rate = processed_rate;
    state->fft_size = fft_size;
    state->raw.start();
    state->processed.start();
    return true;
}
}

extern "C" {

fv1_sdk_result fv1_apple_analysis_create(
    double raw_sample_rate,
    double processed_sample_rate,
    size_t fft_size,
    fv1_apple_analysis_state** out_state) {

    if (!out_state) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    *out_state = nullptr;

    auto* state = new (std::nothrow) fv1_apple_analysis_state();
    if (!state) return FV1_SDK_ERROR_OUT_OF_MEMORY;

    if (!prepare_workers(state, raw_sample_rate, processed_sample_rate, fft_size)) {
        delete state;
        return FV1_SDK_ERROR_BAD_STATE;
    }

    *out_state = state;
    return FV1_SDK_OK;
}

void fv1_apple_analysis_destroy(fv1_apple_analysis_state* state) {
    if (!state) return;
    state->recorder_enabled.store(false, std::memory_order_release);
    state->recorder.stop();
    state->raw.stop();
    state->processed.stop();
    delete state;
}

fv1_sdk_result fv1_apple_analysis_configure(
    fv1_apple_analysis_state* state,
    double raw_sample_rate,
    double processed_sample_rate,
    size_t fft_size) {

    if (!state) return FV1_SDK_ERROR_INVALID_ARGUMENT;

    state->recorder_enabled.store(false, std::memory_order_release);
    state->recorder.stop();
    state->raw.stop();
    state->processed.stop();

    return prepare_workers(state, raw_sample_rate, processed_sample_rate, fft_size)
        ? FV1_SDK_OK
        : FV1_SDK_ERROR_BAD_STATE;
}

void fv1_apple_analysis_push_raw_planar(
    fv1_apple_analysis_state* state,
    const float* left,
    const float* right,
    size_t frames) {

    if (!state || !left || !right || frames == 0u) return;

    fv1::StereoFrame block[kPushChunk];
    std::size_t offset = 0u;

    while (offset < frames) {
        const std::size_t count = std::min(kPushChunk, frames - offset);

        for (std::size_t i = 0u; i < count; ++i) {
            block[i] = {left[offset + i], right[offset + i]};
        }

        state->raw.push(block, count);
        if (state->recorder_enabled.load(std::memory_order_acquire)) {
            state->recorder.push_raw(block, count);
        }
        offset += count;
    }
}

void fv1_apple_analysis_push_raw_sample(
    fv1_apple_analysis_state* state,
    float left,
    float right) {

    if (!state) return;
    const fv1::StereoFrame frame{left, right};
    state->raw.push(&frame, 1u);

    if (state->recorder_enabled.load(std::memory_order_acquire)) {
        state->recorder.push_raw(&frame, 1u);
    }
}

void fv1_apple_analysis_push_processed_sample(
    fv1_apple_analysis_state* state,
    float left,
    float right) {

    if (!state) return;
    const fv1::StereoFrame frame{left, right};
    state->processed.push(&frame, 1u);

    if (state->recorder_enabled.load(std::memory_order_acquire)) {
        state->recorder.push_processed(&frame, 1u);
    }
}

fv1_sdk_result fv1_apple_analysis_copy(
    const fv1_apple_analysis_state* state,
    uint32_t stream,
    fv1_apple_analysis_snapshot_v1* snapshot,
    float* spectrum_db,
    size_t spectrum_capacity,
    float* scope_left,
    float* scope_right,
    size_t scope_capacity) {

    if (!state || !snapshot) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    const auto* analyzer = analyzer_for(state, stream);
    if (!analyzer) return FV1_SDK_ERROR_INVALID_ARGUMENT;

    const fv1::AnalysisSnapshot source = analyzer->latest();
    fv1_apple_analysis_snapshot_v1_init(snapshot);

    snapshot->sample_rate = source.sample_rate;
    snapshot->sequence = source.sequence;
    snapshot->dropped_frames = analyzer->dropped_frames();
    snapshot->peak_left = source.peak_left;
    snapshot->peak_right = source.peak_right;
    snapshot->rms_left = source.rms_left;
    snapshot->rms_right = source.rms_right;
    snapshot->correlation = source.correlation;
    snapshot->dominant_frequency_hz = source.dominant_frequency_hz;
    snapshot->dominant_level_db = source.dominant_level_db;

    const std::size_t spectrum_count = std::min(spectrum_capacity, source.spectrum_db.size());
    snapshot->spectrum_bins = static_cast<std::uint32_t>(spectrum_count);
    if (spectrum_db && spectrum_count > 0u) {
        std::copy_n(source.spectrum_db.data(), spectrum_count, spectrum_db);
    }

    const std::size_t scope_count = std::min(scope_capacity, source.scope_frames.size());
    snapshot->scope_frames = static_cast<std::uint32_t>(scope_count);

    if (scope_left && scope_right) {
        for (std::size_t i = 0u; i < scope_count; ++i) {
            scope_left[i] = source.scope_frames[i].left;
            scope_right[i] = source.scope_frames[i].right;
        }
    }

    return FV1_SDK_OK;
}

fv1_sdk_result fv1_apple_analysis_start_recording(
    fv1_apple_analysis_state* state,
    const char* path,
    uint32_t sample_rate,
    uint32_t mode,
    char* error,
    size_t error_capacity) {

    if (!state || !path || path[0] == '\0' || sample_rate == 0u) {
        copy_error(error, error_capacity, "Invalid Apple recording configuration.");
        return FV1_SDK_ERROR_INVALID_ARGUMENT;
    }

    fv1::AudioRecordMode record_mode = fv1::AudioRecordMode::Processed;
    switch (mode) {
    case FV1_APPLE_RECORD_PROCESSED:
        record_mode = fv1::AudioRecordMode::Processed;
        break;
    case FV1_APPLE_RECORD_RAW:
        record_mode = fv1::AudioRecordMode::Raw;
        break;
    case FV1_APPLE_RECORD_RAW_AND_PROCESSED:
        record_mode = fv1::AudioRecordMode::RawAndProcessed;
        break;
    default:
        copy_error(error, error_capacity, "Unknown Apple recording mode.");
        return FV1_SDK_ERROR_INVALID_ARGUMENT;
    }

    state->recorder_enabled.store(false, std::memory_order_release);
    state->recorder.stop();

    std::string recorder_error;
    if (!state->recorder.prepare(
            std::filesystem::path(path),
            sample_rate,
            record_mode,
            262144u,
            &recorder_error)) {
        copy_error(error, error_capacity, recorder_error);
        return FV1_SDK_ERROR_IO;
    }

    if (!state->recorder.start(&recorder_error)) {
        copy_error(error, error_capacity, recorder_error);
        state->recorder.stop();
        return FV1_SDK_ERROR_IO;
    }

    state->recorder_enabled.store(true, std::memory_order_release);
    copy_error(error, error_capacity, "");
    return FV1_SDK_OK;
}

void fv1_apple_analysis_stop_recording(fv1_apple_analysis_state* state) {
    if (!state) return;
    state->recorder_enabled.store(false, std::memory_order_release);
    state->recorder.stop();
}

uint32_t fv1_apple_analysis_is_recording(const fv1_apple_analysis_state* state) {
    if (!state) return 0u;
    return state->recorder_enabled.load(std::memory_order_acquire) ? 1u : 0u;
}

void fv1_apple_analysis_get_recorder_stats(
    const fv1_apple_analysis_state* state,
    fv1_apple_recorder_stats_v1* stats) {

    if (!stats) return;
    fv1_apple_recorder_stats_v1_init(stats);
    if (!state) return;

    const auto source = state->recorder.stats();
    stats->raw_frames_written = source.raw_frames_written;
    stats->processed_frames_written = source.processed_frames_written;
    stats->raw_frames_dropped = source.raw_frames_dropped;
    stats->processed_frames_dropped = source.processed_frames_dropped;
}

}
