#include <fv1/sdk.h>

#include <fv1/fv1.h>
#include <fv1/spinasm.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include <string>

#ifndef FV1_SDK_VERSION_STRING
#define FV1_SDK_VERSION_STRING "0.8.0"
#endif

struct fv1_sdk_engine {
    fv1_engine* core{};
};

namespace {

fv1_sdk_result map_result(fv1_result result) noexcept {
    switch (result) {
        case FV1_OK: return FV1_SDK_OK;
        case FV1_ERROR_INVALID_ARGUMENT: return FV1_SDK_ERROR_INVALID_ARGUMENT;
        case FV1_ERROR_INVALID_PROGRAM: return FV1_SDK_ERROR_INVALID_PROGRAM;
        case FV1_ERROR_BAD_STATE: return FV1_SDK_ERROR_BAD_STATE;
        case FV1_ERROR_IO: return FV1_SDK_ERROR_IO;
        case FV1_ERROR_UNSUPPORTED: return FV1_SDK_ERROR_UNSUPPORTED;
    }
    return FV1_SDK_ERROR_INTERNAL;
}

bool valid_v1_header(uint32_t struct_size, uint32_t abi_version, size_t required) noexcept {
    return struct_size >= required && (abi_version >> 16) == FV1_SDK_ABI_VERSION_MAJOR;
}

void copy_diagnostic(char* buffer, size_t capacity, const std::string& text) noexcept {
    if (!buffer || capacity == 0) return;
    const size_t count = std::min(capacity - 1, text.size());
    if (count != 0) std::memcpy(buffer, text.data(), count);
    buffer[count] = '\0';
}

} // namespace

extern "C" {

uint32_t fv1_sdk_get_abi_version(void) { return FV1_SDK_ABI_VERSION; }
const char* fv1_sdk_get_version_string(void) { return FV1_SDK_VERSION_STRING; }

const char* fv1_sdk_result_string(fv1_sdk_result result) {
    switch (result) {
        case FV1_SDK_OK: return "ok";
        case FV1_SDK_ERROR_INVALID_ARGUMENT: return "invalid argument";
        case FV1_SDK_ERROR_INVALID_PROGRAM: return "invalid program";
        case FV1_SDK_ERROR_BAD_STATE: return "bad state";
        case FV1_SDK_ERROR_IO: return "I/O error";
        case FV1_SDK_ERROR_UNSUPPORTED: return "unsupported";
        case FV1_SDK_ERROR_OUT_OF_MEMORY: return "out of memory";
        case FV1_SDK_ERROR_COMPILE: return "SpinASM compile error";
        case FV1_SDK_ERROR_INTERNAL: return "internal error";
    }
    return "unknown error";
}

void fv1_sdk_engine_config_v1_init(fv1_sdk_engine_config_v1* config) {
    if (!config) return;
    std::memset(config, 0, sizeof(*config));
    config->struct_size = sizeof(*config);
    config->abi_version = FV1_SDK_ABI_VERSION;
    config->virtual_sample_rate = 32768.0;
    config->delay_model = FV1_SDK_DELAY_REFERENCE_16;
}

void fv1_sdk_snapshot_v1_init(fv1_sdk_snapshot_v1* snapshot) {
    if (!snapshot) return;
    std::memset(snapshot, 0, sizeof(*snapshot));
    snapshot->struct_size = sizeof(*snapshot);
    snapshot->abi_version = FV1_SDK_ABI_VERSION;
}

void fv1_sdk_resource_report_v1_init(fv1_sdk_resource_report_v1* report) {
    if (!report) return;
    std::memset(report, 0, sizeof(*report));
    report->struct_size = sizeof(*report);
    report->abi_version = FV1_SDK_ABI_VERSION;
}

void fv1_sdk_compile_report_v1_init(fv1_sdk_compile_report_v1* report) {
    if (!report) return;
    std::memset(report, 0, sizeof(*report));
    report->struct_size = sizeof(*report);
    report->abi_version = FV1_SDK_ABI_VERSION;
}

fv1_sdk_result fv1_sdk_engine_create_v1(const fv1_sdk_engine_config_v1* config,
                                         fv1_sdk_engine** out_engine) {
    if (!out_engine || !config) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    *out_engine = nullptr;
    if (!valid_v1_header(config->struct_size, config->abi_version, sizeof(*config))) {
        return FV1_SDK_ERROR_UNSUPPORTED;
    }
    if (!(config->virtual_sample_rate > 0.0)) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    if (config->delay_model > FV1_SDK_DELAY_FULL_24) return FV1_SDK_ERROR_INVALID_ARGUMENT;

    auto* wrapper = new (std::nothrow) fv1_sdk_engine{};
    if (!wrapper) return FV1_SDK_ERROR_OUT_OF_MEMORY;
    fv1_config core_config{};
    core_config.virtual_sample_rate = config->virtual_sample_rate;
    core_config.delay_model = config->delay_model == FV1_SDK_DELAY_FULL_24 ? FV1_DELAY_FULL_24 : FV1_DELAY_REFERENCE_16;
    wrapper->core = fv1_create(&core_config);
    if (!wrapper->core) {
        delete wrapper;
        return FV1_SDK_ERROR_OUT_OF_MEMORY;
    }
    *out_engine = wrapper;
    return FV1_SDK_OK;
}

void fv1_sdk_engine_destroy(fv1_sdk_engine* engine) {
    if (!engine) return;
    fv1_destroy(engine->core);
    engine->core = nullptr;
    delete engine;
}

fv1_sdk_result fv1_sdk_engine_reset(fv1_sdk_engine* engine, int clear_delay_ram) {
    if (!engine || !engine->core) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    fv1_reset(engine->core, clear_delay_ram != 0);
    return FV1_SDK_OK;
}

fv1_sdk_result fv1_sdk_engine_load_program(fv1_sdk_engine* engine,
                                            const uint8_t* program,
                                            size_t program_size) {
    if (!engine || !engine->core || !program) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    return map_result(fv1_load_bytes(engine->core, program, program_size));
}

fv1_sdk_result fv1_sdk_engine_set_pots(fv1_sdk_engine* engine,
                                        float pot0, float pot1, float pot2) {
    if (!engine || !engine->core) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    fv1_set_pots(engine->core, pot0, pot1, pot2);
    return FV1_SDK_OK;
}

fv1_sdk_result fv1_sdk_engine_process_planar_f32(fv1_sdk_engine* engine,
                                                  const float* input_left,
                                                  const float* input_right,
                                                  float* output_left,
                                                  float* output_right,
                                                  size_t frames) {
    if (!engine || !engine->core || (!input_left && frames) || (!input_right && frames) ||
        (!output_left && frames) || (!output_right && frames)) {
        return FV1_SDK_ERROR_INVALID_ARGUMENT;
    }
    return map_result(fv1_process_block(engine->core, input_left, input_right, output_left, output_right, frames));
}

fv1_sdk_result fv1_sdk_engine_process_interleaved_f32(fv1_sdk_engine* engine,
                                                       const float* input_stereo,
                                                       float* output_stereo,
                                                       size_t frames) {
    if (!engine || !engine->core || (!input_stereo && frames) || (!output_stereo && frames)) {
        return FV1_SDK_ERROR_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < frames; ++i) {
        float left = 0.0f;
        float right = 0.0f;
        const fv1_result result = fv1_process_sample(engine->core,
                                                     input_stereo[i * 2], input_stereo[i * 2 + 1],
                                                     &left, &right);
        if (result != FV1_OK) return map_result(result);
        output_stereo[i * 2] = left;
        output_stereo[i * 2 + 1] = right;
    }
    return FV1_SDK_OK;
}

fv1_sdk_result fv1_sdk_engine_get_snapshot_v1(const fv1_sdk_engine* engine,
                                               fv1_sdk_snapshot_v1* snapshot) {
    if (!engine || !engine->core || !snapshot) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    if (!valid_v1_header(snapshot->struct_size, snapshot->abi_version, sizeof(*snapshot))) {
        return FV1_SDK_ERROR_UNSUPPORTED;
    }
    fv1_snapshot core{};
    fv1_get_snapshot(engine->core, &core);
    const uint32_t size = snapshot->struct_size;
    const uint32_t version = snapshot->abi_version;
    std::memset(snapshot, 0, sizeof(*snapshot));
    snapshot->struct_size = size;
    snapshot->abi_version = version;
    snapshot->acc = core.acc;
    snapshot->pacc = core.pacc;
    snapshot->lr = core.lr;
    std::copy(std::begin(core.regs), std::end(core.regs), std::begin(snapshot->regs));
    snapshot->delay_pointer = core.delay_pointer;
    std::copy(std::begin(core.sin_lfo), std::end(core.sin_lfo), std::begin(snapshot->sin_lfo));
    std::copy(std::begin(core.cos_lfo), std::end(core.cos_lfo), std::begin(snapshot->cos_lfo));
    std::copy(std::begin(core.ramp_lfo), std::end(core.ramp_lfo), std::begin(snapshot->ramp_lfo));
    snapshot->program_counter = core.program_counter;
    snapshot->first_run = core.first_run;
    snapshot->sample_active = core.debug_sample_active;
    snapshot->sample_counter = core.sample_counter;
    snapshot->instruction_counter = core.instruction_counter;
    return FV1_SDK_OK;
}

fv1_sdk_result fv1_sdk_engine_read_delay_word(const fv1_sdk_engine* engine,
                                               uint32_t address,
                                               int32_t* value) {
    if (!engine || !engine->core || !value) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    return map_result(fv1_read_delay_word(engine->core, address, value));
}

fv1_sdk_result fv1_sdk_engine_analyze_program_v1(const fv1_sdk_engine* engine,
                                                  fv1_sdk_resource_report_v1* report) {
    if (!engine || !engine->core || !report) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    if (!valid_v1_header(report->struct_size, report->abi_version, sizeof(*report))) {
        return FV1_SDK_ERROR_UNSUPPORTED;
    }
    fv1_resource_report core{};
    const fv1_result result = fv1_analyze_program(engine->core, &core);
    if (result != FV1_OK) return map_result(result);
    const uint32_t size = report->struct_size;
    const uint32_t version = report->abi_version;
    std::memset(report, 0, sizeof(*report));
    report->struct_size = size;
    report->abi_version = version;
    report->used_instructions = core.used_instructions;
    report->worst_case_path = core.worst_case_path;
    report->static_delay_reads = core.static_delay_reads;
    report->static_delay_writes = core.static_delay_writes;
    report->dynamic_delay_reads = core.dynamic_delay_reads;
    report->highest_static_delay_address = core.highest_static_delay_address;
    report->general_registers_used = core.general_registers_used;
    report->pots_used = core.pots_used;
    report->sine_lfos_used = core.sine_lfos_used;
    report->ramp_lfos_used = core.ramp_lfos_used;
    report->skip_instructions = core.skip_instructions;
    std::copy(std::begin(core.opcode_histogram), std::end(core.opcode_histogram), std::begin(report->opcode_histogram));
    return FV1_SDK_OK;
}

fv1_sdk_result fv1_sdk_compile_spinasm_v1(const char* source_utf8,
                                           size_t source_size,
                                           uint8_t* output_program,
                                           size_t output_capacity,
                                           fv1_sdk_compile_report_v1* report,
                                           char* diagnostic_utf8,
                                           size_t diagnostic_capacity) {
    if (diagnostic_utf8 && diagnostic_capacity) diagnostic_utf8[0] = '\0';
    if (!source_utf8 || !output_program || output_capacity < FV1_SDK_PROGRAM_BYTES) {
        copy_diagnostic(diagnostic_utf8, diagnostic_capacity, "invalid compiler arguments");
        return FV1_SDK_ERROR_INVALID_ARGUMENT;
    }
    if (report && !valid_v1_header(report->struct_size, report->abi_version, sizeof(*report))) {
        copy_diagnostic(diagnostic_utf8, diagnostic_capacity, "unsupported compile-report ABI version");
        return FV1_SDK_ERROR_UNSUPPORTED;
    }
    try {
        const auto compiled = fv1::spinasm::compile(std::string_view(source_utf8, source_size));
        std::copy(compiled.image.begin(), compiled.image.end(), output_program);
        if (report) {
            const uint32_t size = report->struct_size;
            const uint32_t version = report->abi_version;
            std::memset(report, 0, sizeof(*report));
            report->struct_size = size;
            report->abi_version = version;
            report->instruction_count = compiled.instruction_count;
            report->highest_delay_address = compiled.highest_delay_address;
        }
        return FV1_SDK_OK;
    } catch (const fv1::spinasm::CompileError& error) {
        if (report) report->error_line = error.line();
        copy_diagnostic(diagnostic_utf8, diagnostic_capacity, error.what());
        return FV1_SDK_ERROR_COMPILE;
    } catch (const std::bad_alloc&) {
        copy_diagnostic(diagnostic_utf8, diagnostic_capacity, "out of memory while compiling SpinASM");
        return FV1_SDK_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception& error) {
        copy_diagnostic(diagnostic_utf8, diagnostic_capacity, error.what());
        return FV1_SDK_ERROR_INTERNAL;
    } catch (...) {
        copy_diagnostic(diagnostic_utf8, diagnostic_capacity, "unknown SpinASM compiler failure");
        return FV1_SDK_ERROR_INTERNAL;
    }
}

} // extern "C"
