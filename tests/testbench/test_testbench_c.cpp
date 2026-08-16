#include <fv1/testbench.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {

bool fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    return false;
}

} // namespace

int main() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "fv1-phase8c-testbench";

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);

    char error[1024]{};

    if (fv1_testbench_write_validation_pack(
            root.string().c_str(),
            48000u,
            0.25,
            0.20,
            0x465631u,
            error,
            sizeof(error))
        != FV1_TESTBENCH_OK) {
        std::fprintf(
            stderr,
            "validation pack generation failed: %s\n",
            error);
        return 1;
    }

    const auto sine =
        root / "04-sine-1khz.wav";

    if (!std::filesystem::exists(sine)) {
        return fail(
            "validation pack did not create 04-sine-1khz.wav")
            ? 0 : 2;
    }

    fv1_testbench_file_source* file = nullptr;
    if (fv1_testbench_file_source_create(&file)
            != FV1_TESTBENCH_OK
        || !file) {
        return fail(
            "file-loop source create failed")
            ? 0 : 3;
    }

    if (fv1_testbench_file_source_load(
            file,
            sine.string().c_str(),
            error,
            sizeof(error))
        != FV1_TESTBENCH_OK) {
        std::fprintf(
            stderr,
            "file-loop load failed: %s\n",
            error);
        fv1_testbench_file_source_destroy(file);
        return 4;
    }

    if (fv1_testbench_file_source_prepare(
            file,
            48000.0,
            512u)
        != FV1_TESTBENCH_OK) {
        fv1_testbench_file_source_destroy(file);
        return fail(
            "file-loop prepare failed")
            ? 0 : 5;
    }

    fv1_testbench_file_info_v1 info;
    fv1_testbench_file_info_v1_init(&info);
    if (fv1_testbench_file_source_get_info(
            file,
            &info)
            != FV1_TESTBENCH_OK
        || info.file_sample_rate != 48000u
        || info.duration_seconds <= 0.0
        || info.total_frames == 0u) {
        fv1_testbench_file_source_destroy(file);
        return fail(
            "file-loop metadata mismatch")
            ? 0 : 6;
    }

    fv1_testbench_file_source_set_looping(
        file,
        1u);
    fv1_testbench_file_source_set_crossfade_ms(
        file,
        5.0);

    if (fv1_testbench_file_source_set_loop_region_seconds(
            file,
            0.02,
            std::min(
                info.duration_seconds,
                0.20))
        != FV1_TESTBENCH_OK) {
        fv1_testbench_file_source_destroy(file);
        return fail(
            "file-loop region failed")
            ? 0 : 7;
    }

    if (fv1_testbench_file_source_seek_seconds(
            file,
            0.03)
        != FV1_TESTBENCH_OK) {
        fv1_testbench_file_source_destroy(file);
        return fail(
            "file-loop seek failed")
            ? 0 : 8;
    }

    fv1_testbench_file_source_play(file);

    float left[512]{};
    float right[512]{};

    if (fv1_testbench_file_source_render_planar(
            file,
            left,
            right,
            512u)
        != FV1_TESTBENCH_OK) {
        fv1_testbench_file_source_destroy(file);
        return fail(
            "file-loop render failed")
            ? 0 : 9;
    }

    double energy = 0.0;
    for (std::size_t i = 0u; i < 512u; ++i) {
        energy +=
            static_cast<double>(left[i])
                * static_cast<double>(left[i]);
        energy +=
            static_cast<double>(right[i])
                * static_cast<double>(right[i]);
    }

    if (energy <= 1.0e-5) {
        fv1_testbench_file_source_destroy(file);
        return fail(
            "file-loop rendered silence")
            ? 0 : 10;
    }

    fv1_testbench_file_source_pause(file);
    fv1_testbench_file_source_get_info(file, &info);
    if (info.transport_state
        != FV1_TESTBENCH_TRANSPORT_PAUSED) {
        fv1_testbench_file_source_destroy(file);
        return fail(
            "file-loop pause state mismatch")
            ? 0 : 11;
    }

    fv1_testbench_file_source_stop(file);
    fv1_testbench_file_source_get_info(file, &info);
    if (info.transport_state
        != FV1_TESTBENCH_TRANSPORT_STOPPED
        || info.position_seconds > 0.001) {
        fv1_testbench_file_source_destroy(file);
        return fail(
            "file-loop stop/reset mismatch")
            ? 0 : 12;
    }

    fv1_testbench_file_source_destroy(file);

    fv1_testbench_validation_config_v1 config;
    fv1_testbench_validation_config_v1_init(&config);

    fv1_testbench_validation_summary_v1 summary;
    fv1_testbench_validation_summary_v1_init(&summary);

    char failures[2048]{};

    if (fv1_testbench_validate_wavs(
            sine.string().c_str(),
            sine.string().c_str(),
            &config,
            nullptr,
            &summary,
            failures,
            sizeof(failures),
            error,
            sizeof(error))
            != FV1_TESTBENCH_OK
        || summary.passed == 0u
        || summary.left_correlation < 0.9999
        || summary.right_correlation < 0.9999) {
        std::fprintf(
            stderr,
            "self-validation failed: error='%s' failures='%s' Lcorr=%f Rcorr=%f\n",
            error,
            failures,
            summary.left_correlation,
            summary.right_correlation);
        return 13;
    }

    std::filesystem::remove_all(root, ec);
    std::puts(
        "Phase 8C testbench services OK");
    return 0;
}
