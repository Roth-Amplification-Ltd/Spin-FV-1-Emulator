#include <fv1/testbench.h>

#include <fv1/audio_source.hpp>
#include <fv1/validation.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <new>
#include <sstream>
#include <string>

namespace {

constexpr std::size_t kRenderChunk = 512u;

void copy_text(
    char* output,
    std::size_t capacity,
    const std::string& text) {
    if (!output || capacity == 0u) return;
    std::snprintf(output, capacity, "%s", text.c_str());
}

std::uint32_t transport_value(
    fv1::TransportState state) noexcept {
    switch (state) {
    case fv1::TransportState::Playing:
        return FV1_TESTBENCH_TRANSPORT_PLAYING;
    case fv1::TransportState::Paused:
        return FV1_TESTBENCH_TRANSPORT_PAUSED;
    case fv1::TransportState::Stopped:
    default:
        return FV1_TESTBENCH_TRANSPORT_STOPPED;
    }
}

fv1::ValidationConfig validation_config_from_c(
    const fv1_testbench_validation_config_v1* source) {
    fv1::ValidationConfig config;
    if (!source) return config;

    config.max_alignment_ms =
        source->max_alignment_ms;
    config.gain_match_residual =
        source->gain_match_residual != 0u;
    config.fft_size =
        static_cast<std::size_t>(source->fft_size);
    config.spectral_floor_db =
        source->spectral_floor_db;
    config.minimum_correlation =
        source->minimum_correlation;
    config.maximum_residual_rms_dbfs =
        source->maximum_residual_rms_dbfs;
    config.maximum_residual_peak_dbfs =
        source->maximum_residual_peak_dbfs;
    return config;
}

void copy_channel(
    const fv1::ValidationChannelMetrics& source,
    double& reference_rms,
    double& capture_rms,
    double& gain_error,
    double& correlation,
    double& residual_rms,
    double& residual_peak,
    double& snr) {
    reference_rms = source.reference_rms_dbfs;
    capture_rms = source.capture_rms_dbfs;
    gain_error = source.gain_error_db;
    correlation = source.correlation;
    residual_rms = source.residual_rms_dbfs;
    residual_peak = source.residual_peak_dbfs;
    snr = source.snr_db;
}

} // namespace

struct fv1_testbench_file_source {
    fv1::FileLoopSource source;
    bool loaded{false};
    bool prepared{false};
};

extern "C" {

void fv1_testbench_file_info_v1_init(
    fv1_testbench_file_info_v1* info) {
    if (!info) return;
    std::memset(info, 0, sizeof(*info));
    info->struct_size =
        static_cast<std::uint32_t>(sizeof(*info));
}

fv1_testbench_result fv1_testbench_file_source_create(
    fv1_testbench_file_source** out_source) {
    if (!out_source) {
        return FV1_TESTBENCH_ERROR_INVALID_ARGUMENT;
    }
    *out_source = nullptr;

    auto* source =
        new (std::nothrow) fv1_testbench_file_source();
    if (!source) {
        return FV1_TESTBENCH_ERROR_OUT_OF_MEMORY;
    }

    *out_source = source;
    return FV1_TESTBENCH_OK;
}

void fv1_testbench_file_source_destroy(
    fv1_testbench_file_source* source) {
    delete source;
}

fv1_testbench_result fv1_testbench_file_source_load(
    fv1_testbench_file_source* source,
    const char* path_utf8,
    char* error_utf8,
    size_t error_capacity) {
    if (!source || !path_utf8 || path_utf8[0] == '\0') {
        copy_text(
            error_utf8,
            error_capacity,
            "A WAV path is required.");
        return FV1_TESTBENCH_ERROR_INVALID_ARGUMENT;
    }

    std::string error;
    const bool ok = source->source.load(
        std::filesystem::path(path_utf8),
        &error);
    if (!ok) {
        copy_text(error_utf8, error_capacity, error);
        source->loaded = false;
        source->prepared = false;
        return FV1_TESTBENCH_ERROR_IO;
    }

    source->loaded = true;
    source->prepared = false;
    copy_text(error_utf8, error_capacity, "");
    return FV1_TESTBENCH_OK;
}

fv1_testbench_result fv1_testbench_file_source_prepare(
    fv1_testbench_file_source* source,
    double host_sample_rate,
    size_t max_block_frames) {
    if (!source || !source->loaded
        || !(host_sample_rate > 1000.0)
        || max_block_frames == 0u) {
        return FV1_TESTBENCH_ERROR_INVALID_ARGUMENT;
    }

    if (!source->source.prepare(
            host_sample_rate,
            max_block_frames)) {
        source->prepared = false;
        return FV1_TESTBENCH_ERROR_BAD_STATE;
    }

    source->prepared = true;
    return FV1_TESTBENCH_OK;
}

fv1_testbench_result fv1_testbench_file_source_render_planar(
    fv1_testbench_file_source* source,
    float* output_left,
    float* output_right,
    size_t frames) {
    if (!source || !source->prepared
        || !output_left || !output_right) {
        return FV1_TESTBENCH_ERROR_INVALID_ARGUMENT;
    }

    fv1::StereoFrame block[kRenderChunk];
    std::size_t offset = 0u;

    while (offset < frames) {
        const std::size_t count =
            std::min(kRenderChunk, frames - offset);

        source->source.render(
            nullptr,
            block,
            count);

        for (std::size_t i = 0u; i < count; ++i) {
            output_left[offset + i] =
                block[i].left;
            output_right[offset + i] =
                block[i].right;
        }

        offset += count;
    }

    return FV1_TESTBENCH_OK;
}

void fv1_testbench_file_source_play(
    fv1_testbench_file_source* source) {
    if (source) source->source.play();
}

void fv1_testbench_file_source_pause(
    fv1_testbench_file_source* source) {
    if (source) source->source.pause();
}

void fv1_testbench_file_source_stop(
    fv1_testbench_file_source* source) {
    if (source) source->source.stop();
}

void fv1_testbench_file_source_set_looping(
    fv1_testbench_file_source* source,
    uint32_t enabled) {
    if (source) {
        source->source.set_looping(enabled != 0u);
    }
}

fv1_testbench_result fv1_testbench_file_source_seek_seconds(
    fv1_testbench_file_source* source,
    double seconds) {
    if (!source || !source->loaded
        || !std::isfinite(seconds)) {
        return FV1_TESTBENCH_ERROR_INVALID_ARGUMENT;
    }

    return source->source.seek_seconds(seconds)
        ? FV1_TESTBENCH_OK
        : FV1_TESTBENCH_ERROR_BAD_STATE;
}

fv1_testbench_result fv1_testbench_file_source_set_loop_region_seconds(
    fv1_testbench_file_source* source,
    double begin_seconds,
    double end_seconds) {
    if (!source || !source->loaded
        || !std::isfinite(begin_seconds)
        || !std::isfinite(end_seconds)
        || begin_seconds < 0.0
        || end_seconds <= begin_seconds) {
        return FV1_TESTBENCH_ERROR_INVALID_ARGUMENT;
    }

    const std::uint32_t rate =
        source->source.file_sample_rate();
    const std::uint64_t total =
        source->source.total_frames();

    if (rate == 0u || total == 0u) {
        return FV1_TESTBENCH_ERROR_BAD_STATE;
    }

    const double rate_d =
        static_cast<double>(rate);

    const auto begin = static_cast<std::uint64_t>(
        std::llround(begin_seconds * rate_d));
    const auto end = static_cast<std::uint64_t>(
        std::llround(end_seconds * rate_d));

    if (begin >= end || end > total) {
        return FV1_TESTBENCH_ERROR_INVALID_ARGUMENT;
    }

    return source->source.set_loop_region(begin, end)
        ? FV1_TESTBENCH_OK
        : FV1_TESTBENCH_ERROR_BAD_STATE;
}

void fv1_testbench_file_source_set_crossfade_ms(
    fv1_testbench_file_source* source,
    double milliseconds) {
    if (source) {
        source->source.set_loop_crossfade_ms(
            milliseconds);
    }
}

fv1_testbench_result fv1_testbench_file_source_get_info(
    const fv1_testbench_file_source* source,
    fv1_testbench_file_info_v1* info) {
    if (!source || !info) {
        return FV1_TESTBENCH_ERROR_INVALID_ARGUMENT;
    }

    fv1_testbench_file_info_v1_init(info);
    info->transport_state =
        transport_value(source->source.state());
    info->looping =
        source->source.looping() ? 1u : 0u;
    info->file_sample_rate =
        source->source.file_sample_rate();
    info->total_frames =
        source->source.total_frames();
    info->duration_seconds =
        source->source.duration_seconds();
    info->position_seconds =
        source->source.position_seconds();
    info->loop_begin_seconds =
        source->source.loop_begin_seconds();
    info->loop_end_seconds =
        source->source.loop_end_seconds();
    info->loop_crossfade_ms =
        source->source.loop_crossfade_ms();
    return FV1_TESTBENCH_OK;
}


void fv1_testbench_validation_config_v1_init(
    fv1_testbench_validation_config_v1* config) {
    if (!config) return;
    std::memset(config, 0, sizeof(*config));
    config->struct_size =
        static_cast<std::uint32_t>(sizeof(*config));
    config->max_alignment_ms = 100.0;
    config->gain_match_residual = 0u;
    config->fft_size = 16384u;
    config->spectral_floor_db = -90.0;
    config->minimum_correlation = 0.995;
    config->maximum_residual_rms_dbfs = -45.0;
    config->maximum_residual_peak_dbfs = -24.0;
}

void fv1_testbench_validation_summary_v1_init(
    fv1_testbench_validation_summary_v1* summary) {
    if (!summary) return;
    std::memset(summary, 0, sizeof(*summary));
    summary->struct_size =
        static_cast<std::uint32_t>(sizeof(*summary));
}

fv1_testbench_result fv1_testbench_validate_wavs(
    const char* reference_path_utf8,
    const char* capture_path_utf8,
    const fv1_testbench_validation_config_v1* config,
    const char* report_prefix_utf8,
    fv1_testbench_validation_summary_v1* summary,
    char* failure_text_utf8,
    size_t failure_text_capacity,
    char* error_utf8,
    size_t error_capacity) {
    if (!reference_path_utf8
        || !capture_path_utf8
        || !summary) {
        return FV1_TESTBENCH_ERROR_INVALID_ARGUMENT;
    }

    fv1::ValidationAudio reference;
    fv1::ValidationAudio capture;
    std::string error;

    if (!fv1::load_validation_wav(
            std::filesystem::path(reference_path_utf8),
            reference,
            &error)) {
        copy_text(error_utf8, error_capacity, error);
        return FV1_TESTBENCH_ERROR_IO;
    }

    if (!fv1::load_validation_wav(
            std::filesystem::path(capture_path_utf8),
            capture,
            &error)) {
        copy_text(error_utf8, error_capacity, error);
        return FV1_TESTBENCH_ERROR_IO;
    }

    const fv1::ValidationResult result =
        fv1::validate_recordings(
            reference,
            capture,
            validation_config_from_c(config));

    fv1_testbench_validation_summary_v1_init(summary);
    summary->passed = result.passed ? 1u : 0u;
    summary->sample_rate = result.sample_rate;
    summary->failure_count =
        static_cast<std::uint32_t>(result.failures.size());
    summary->capture_delay_frames =
        result.capture_delay_frames;
    summary->compared_frames =
        static_cast<std::uint64_t>(result.compared_frames);
    summary->capture_delay_ms =
        result.capture_delay_ms;
    summary->applied_capture_gain_db =
        result.applied_capture_gain_db;

    copy_channel(
        result.left,
        summary->left_reference_rms_dbfs,
        summary->left_capture_rms_dbfs,
        summary->left_gain_error_db,
        summary->left_correlation,
        summary->left_residual_rms_dbfs,
        summary->left_residual_peak_dbfs,
        summary->left_snr_db);

    copy_channel(
        result.right,
        summary->right_reference_rms_dbfs,
        summary->right_capture_rms_dbfs,
        summary->right_gain_error_db,
        summary->right_correlation,
        summary->right_residual_rms_dbfs,
        summary->right_residual_peak_dbfs,
        summary->right_snr_db);

    summary->spectral_rms_magnitude_error_db =
        result.spectral_rms_magnitude_error_db;
    summary->spectral_worst_magnitude_error_db =
        result.spectral_worst_magnitude_error_db;
    summary->spectral_worst_phase_error_degrees =
        result.spectral_worst_phase_error_degrees;

    std::ostringstream failures;
    for (std::size_t i = 0u;
         i < result.failures.size();
         ++i) {
        if (i != 0u) failures << '\n';
        failures << result.failures[i];
    }
    copy_text(
        failure_text_utf8,
        failure_text_capacity,
        failures.str());

    if (report_prefix_utf8
        && report_prefix_utf8[0] != '\0') {
        if (!fv1::write_validation_report_bundle(
                std::filesystem::path(report_prefix_utf8),
                result,
                &error)) {
            copy_text(error_utf8, error_capacity, error);
            return FV1_TESTBENCH_ERROR_IO;
        }
    }

    copy_text(error_utf8, error_capacity, "");
    return FV1_TESTBENCH_OK;
}

fv1_testbench_result fv1_testbench_write_validation_pack(
    const char* directory_utf8,
    uint32_t sample_rate,
    double seconds,
    double level,
    uint32_t seed,
    char* error_utf8,
    size_t error_capacity) {
    if (!directory_utf8
        || directory_utf8[0] == '\0'
        || sample_rate == 0u
        || !(seconds > 0.0)
        || !std::isfinite(seconds)
        || !std::isfinite(level)) {
        return FV1_TESTBENCH_ERROR_INVALID_ARGUMENT;
    }

    fv1::ValidationPackConfig config;
    config.sample_rate = sample_rate;
    config.standard_seconds = seconds;
    config.level = std::clamp(level, 0.0, 1.0);
    config.seed = seed;

    std::string error;
    if (!fv1::write_validation_stimulus_pack(
            std::filesystem::path(directory_utf8),
            config,
            &error)) {
        copy_text(error_utf8, error_capacity, error);
        return FV1_TESTBENCH_ERROR_IO;
    }

    copy_text(error_utf8, error_capacity, "");
    return FV1_TESTBENCH_OK;
}

} // extern "C"
