#include <fv1/sdk.h>
#include <fv1/sdk_debug.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

float float_from_bytes(const std::uint8_t* data, std::size_t size, std::size_t offset) {
    std::uint32_t bits = 0;
    for (std::size_t i = 0; i < 4 && offset + i < size; ++i) {
        bits |= static_cast<std::uint32_t>(data[offset + i]) << (8u * static_cast<unsigned>(i));
    }
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (!data && size != 0) return 0;

    fv1_sdk_engine_config_v1 config;
    fv1_sdk_engine_config_v1_init(&config);
    if (size > 0) config.delay_model = data[0] & 1u;
    if (size >= 9) {
        double rate = 0.0;
        std::memcpy(&rate, data + 1, sizeof(rate));
        config.virtual_sample_rate = rate;
    }

    fv1_sdk_engine* engine = nullptr;
    if (fv1_sdk_engine_create_v1(&config, &engine) != FV1_SDK_OK || !engine) {
        /* Most arbitrary bit patterns are intentionally invalid clocks. */
        return 0;
    }

    std::array<std::uint8_t, FV1_SDK_PROGRAM_BYTES> program{};
    bool loaded = false;
    if (size >= FV1_SDK_PROGRAM_BYTES + 9u) {
        std::copy_n(data + 9u, FV1_SDK_PROGRAM_BYTES, program.begin());
        loaded = fv1_sdk_engine_load_program(engine, program.data(), program.size()) == FV1_SDK_OK;
    } else {
        const std::size_t source_offset = std::min<std::size_t>(9u, size);
        const std::size_t source_size = std::min<std::size_t>(size - source_offset, 4096u);
        fv1_sdk_compile_report_v1 report;
        fv1_sdk_compile_report_v1_init(&report);
        char diagnostic[64];
        if (fv1_sdk_compile_spinasm_v1(
                reinterpret_cast<const char*>(data + source_offset), source_size,
                program.data(), program.size(), &report,
                diagnostic, sizeof(diagnostic)) == FV1_SDK_OK) {
            loaded = fv1_sdk_engine_load_program(engine, program.data(), program.size()) == FV1_SDK_OK;
        }
    }

    const float p0 = float_from_bytes(data, size, 0);
    const float p1 = float_from_bytes(data, size, 4);
    const float p2 = float_from_bytes(data, size, 8);
    (void)fv1_sdk_engine_set_pots(engine, p0, p1, p2);
    (void)fv1_sdk_engine_set_pot(engine, size ? data[size - 1] % 5u : 0u, p1);

    if (loaded) {
        constexpr std::size_t max_frames = 32;
        std::array<float, max_frames * 2u> input{};
        std::array<float, max_frames * 2u> output{};
        const std::size_t frames = size ? data[0] % (max_frames + 1u) : 0u;
        for (std::size_t i = 0; i < frames * 2u; ++i) {
            input[i] = float_from_bytes(data, size, (i * 3u) % (size ? size : 1u));
        }
        (void)fv1_sdk_engine_process_interleaved_f32(engine,
                                                      frames ? input.data() : nullptr,
                                                      frames ? output.data() : nullptr,
                                                      frames);

        fv1_sdk_snapshot_v1 snapshot;
        fv1_sdk_snapshot_v1_init(&snapshot);
        (void)fv1_sdk_engine_get_snapshot_v1(engine, &snapshot);

        fv1_sdk_resource_report_v1 resources;
        fv1_sdk_resource_report_v1_init(&resources);
        (void)fv1_sdk_engine_analyze_program_v1(engine, &resources);

        int32_t delay = 0;
        const std::uint32_t address = size > 1
            ? (static_cast<std::uint32_t>(data[0]) << 8u | data[1])
            : 0u;
        (void)fv1_sdk_engine_read_delay_word(engine, address, &delay);

        if (fv1_sdk_debug_begin_sample(engine, p0, p1) == FV1_SDK_OK) {
            for (unsigned step = 0; step < 8u; ++step) {
                fv1_sdk_trace_v1 trace;
                fv1_sdk_trace_v1_init(&trace);
                if (fv1_sdk_debug_step_instruction_v1(engine, &trace) != FV1_SDK_OK) break;
                if (trace.sample_finished) {
                    float left = 0.0f, right = 0.0f;
                    (void)fv1_sdk_debug_finish_sample(engine, &left, &right);
                    break;
                }
            }
        }

        fv1_sdk_state_digest_v1 digest;
        fv1_sdk_state_digest_v1_init(&digest);
        (void)fv1_sdk_engine_get_state_digest_v1(engine, &digest);
    }

    (void)fv1_sdk_engine_reset(engine, size > 2 ? data[2] : 0u);
    fv1_sdk_engine_destroy(engine);
    return 0;
}
