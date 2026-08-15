#include <fv1/sdk.h>
#include <fv1/sdk_debug.h>

#include <fv1/fv1.h>
#include <fv1/spinasm.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include <string>

#ifndef FV1_SDK_VERSION_STRING
#define FV1_SDK_VERSION_STRING "0.9.0"
#endif
#ifndef FV1_SDK_VERSION_MAJOR_VALUE
#define FV1_SDK_VERSION_MAJOR_VALUE 0
#endif
#ifndef FV1_SDK_VERSION_MINOR_VALUE
#define FV1_SDK_VERSION_MINOR_VALUE 9
#endif
#ifndef FV1_SDK_VERSION_PATCH_VALUE
#define FV1_SDK_VERSION_PATCH_VALUE 0
#endif

struct fv1_sdk_engine {
    fv1_engine* core{};
    float pots[3]{};
};

namespace {

constexpr std::uint64_t sdk_capabilities =
    FV1_SDK_CAP_CORE_PROCESSING |
    FV1_SDK_CAP_SPINASM_COMPILER |
    FV1_SDK_CAP_SNAPSHOT |
    FV1_SDK_CAP_DELAY_INSPECTION |
    FV1_SDK_CAP_RESOURCE_ANALYSIS |
    FV1_SDK_CAP_DEBUG_STEPPING |
    FV1_SDK_CAP_STATE_DIGEST |
    FV1_SDK_CAP_PLANAR_F32 |
    FV1_SDK_CAP_INTERLEAVED_F32 |
    FV1_SDK_CAP_PROGRAM_READBACK;

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

template <typename T>
void init_v1(T* value) noexcept {
    if (!value) return;
    std::memset(value, 0, sizeof(*value));
    value->struct_size = sizeof(*value);
    value->abi_version = FV1_SDK_ABI_VERSION;
}

std::size_t copy_diagnostic(char* buffer, size_t capacity, const std::string& text) noexcept {
    if (!buffer || capacity == 0) return 0;
    const size_t count = std::min(capacity - 1, text.size());
    if (count != 0) std::memcpy(buffer, text.data(), count);
    buffer[count] = '\0';
    return count;
}

void publish_diagnostic(fv1_sdk_compile_report_v1* report, char* buffer, size_t capacity,
                        const std::string& text) noexcept {
    const std::size_t written = copy_diagnostic(buffer, capacity, text);
    if (!report) return;
    report->diagnostic_bytes_required = static_cast<uint32_t>(
        std::min<std::size_t>(text.size() + 1u, UINT32_MAX));
    report->diagnostic_bytes_written = static_cast<uint32_t>(
        std::min<std::size_t>(written, UINT32_MAX));
}

} // namespace

extern "C" {

uint32_t fv1_sdk_get_abi_version(void) { return FV1_SDK_ABI_VERSION; }
uint64_t fv1_sdk_get_capabilities(void) { return sdk_capabilities; }
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

void fv1_sdk_version_info_v1_init(fv1_sdk_version_info_v1* info) { init_v1(info); }

fv1_sdk_result fv1_sdk_get_version_info_v1(fv1_sdk_version_info_v1* info) {
    if (!info) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    if (!valid_v1_header(info->struct_size, info->abi_version, sizeof(*info))) {
        return FV1_SDK_ERROR_UNSUPPORTED;
    }
    const uint32_t size = info->struct_size;
    const uint32_t version = info->abi_version;
    std::memset(info, 0, sizeof(*info));
    info->struct_size = size;
    info->abi_version = version;
    info->sdk_version_major = FV1_SDK_VERSION_MAJOR_VALUE;
    info->sdk_version_minor = FV1_SDK_VERSION_MINOR_VALUE;
    info->sdk_version_patch = FV1_SDK_VERSION_PATCH_VALUE;
    info->capabilities = sdk_capabilities;
    info->program_bytes = FV1_SDK_PROGRAM_BYTES;
    info->register_count = FV1_SDK_REGISTER_COUNT;
    info->delay_words = FV1_SDK_DELAY_WORDS;
    return FV1_SDK_OK;
}

void fv1_sdk_engine_config_v1_init(fv1_sdk_engine_config_v1* config) {
    init_v1(config);
    if (!config) return;
    config->virtual_sample_rate = 32768.0;
    config->delay_model = FV1_SDK_DELAY_REFERENCE_16;
}

void fv1_sdk_snapshot_v1_init(fv1_sdk_snapshot_v1* snapshot) { init_v1(snapshot); }
void fv1_sdk_resource_report_v1_init(fv1_sdk_resource_report_v1* report) { init_v1(report); }
void fv1_sdk_compile_report_v1_init(fv1_sdk_compile_report_v1* report) { init_v1(report); }
void fv1_sdk_trace_v1_init(fv1_sdk_trace_v1* trace) { init_v1(trace); }
void fv1_sdk_state_digest_v1_init(fv1_sdk_state_digest_v1* digest) { init_v1(digest); }

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

fv1_sdk_result fv1_sdk_engine_reset(fv1_sdk_engine* engine, uint32_t clear_delay_ram) {
    if (!engine || !engine->core) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    fv1_reset(engine->core, clear_delay_ram != 0);
    fv1_set_pots(engine->core, engine->pots[0], engine->pots[1], engine->pots[2]);
    return FV1_SDK_OK;
}

fv1_sdk_result fv1_sdk_engine_load_program(fv1_sdk_engine* engine,
                                            const uint8_t* program,
                                            size_t program_size) {
    if (!engine || !engine->core || !program) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    return map_result(fv1_load_bytes(engine->core, program, program_size));
}

fv1_sdk_result fv1_sdk_engine_get_program(const fv1_sdk_engine* engine,
                                           uint8_t* output_program,
                                           size_t output_capacity) {
    if (!engine || !engine->core || !output_program || output_capacity < FV1_SDK_PROGRAM_BYTES) {
        return FV1_SDK_ERROR_INVALID_ARGUMENT;
    }
    std::array<uint32_t, FV1_SDK_PROGRAM_BYTES / 4u> words{};
    const fv1_result result = fv1_get_program_words(engine->core, words.data(), words.size());
    if (result != FV1_OK) return map_result(result);
    for (std::size_t i = 0; i < words.size(); ++i) {
        const uint32_t word = words[i];
        output_program[i * 4u] = static_cast<uint8_t>((word >> 24u) & 0xffu);
        output_program[i * 4u + 1u] = static_cast<uint8_t>((word >> 16u) & 0xffu);
        output_program[i * 4u + 2u] = static_cast<uint8_t>((word >> 8u) & 0xffu);
        output_program[i * 4u + 3u] = static_cast<uint8_t>(word & 0xffu);
    }
    return FV1_SDK_OK;
}

fv1_sdk_result fv1_sdk_engine_set_pot(fv1_sdk_engine* engine,
                                       uint32_t index,
                                       float value) {
    if (!engine || !engine->core || index > 2u) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    engine->pots[index] = value;
    fv1_set_pots(engine->core, engine->pots[0], engine->pots[1], engine->pots[2]);
    return FV1_SDK_OK;
}

fv1_sdk_result fv1_sdk_engine_set_pots(fv1_sdk_engine* engine,
                                        float pot0, float pot1, float pot2) {
    if (!engine || !engine->core) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    engine->pots[0] = pot0;
    engine->pots[1] = pot1;
    engine->pots[2] = pot2;
    fv1_set_pots(engine->core, pot0, pot1, pot2);
    return FV1_SDK_OK;
}

fv1_sdk_result fv1_sdk_engine_process_sample_f32(fv1_sdk_engine* engine,
                                                  float input_left,
                                                  float input_right,
                                                  float* output_left,
                                                  float* output_right) {
    if (!engine || !engine->core || !output_left || !output_right) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    return map_result(fv1_process_sample(engine->core, input_left, input_right, output_left, output_right));
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
        const fv1_sdk_result result = fv1_sdk_engine_process_sample_f32(
            engine, input_stereo[i * 2u], input_stereo[i * 2u + 1u],
            &output_stereo[i * 2u], &output_stereo[i * 2u + 1u]);
        if (result != FV1_SDK_OK) return result;
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
    if (report && !valid_v1_header(report->struct_size, report->abi_version, sizeof(*report))) {
        copy_diagnostic(diagnostic_utf8, diagnostic_capacity, "unsupported compile-report ABI version");
        return FV1_SDK_ERROR_UNSUPPORTED;
    }
    if (report) {
        const uint32_t size = report->struct_size;
        const uint32_t version = report->abi_version;
        std::memset(report, 0, sizeof(*report));
        report->struct_size = size;
        report->abi_version = version;
    }
    if (!source_utf8 || !output_program || output_capacity < FV1_SDK_PROGRAM_BYTES) {
        publish_diagnostic(report, diagnostic_utf8, diagnostic_capacity, "invalid compiler arguments");
        return FV1_SDK_ERROR_INVALID_ARGUMENT;
    }
    try {
        const auto compiled = fv1::spinasm::compile(std::string_view(source_utf8, source_size));
        std::copy(compiled.image.begin(), compiled.image.end(), output_program);
        if (report) {
            report->instruction_count = compiled.instruction_count;
            report->highest_delay_address = compiled.highest_delay_address;
        }
        return FV1_SDK_OK;
    } catch (const fv1::spinasm::CompileError& error) {
        if (report) report->error_line = error.line();
        publish_diagnostic(report, diagnostic_utf8, diagnostic_capacity, error.what());
        return FV1_SDK_ERROR_COMPILE;
    } catch (const std::bad_alloc&) {
        publish_diagnostic(report, diagnostic_utf8, diagnostic_capacity, "out of memory while compiling SpinASM");
        return FV1_SDK_ERROR_OUT_OF_MEMORY;
    } catch (const std::exception& error) {
        publish_diagnostic(report, diagnostic_utf8, diagnostic_capacity, error.what());
        return FV1_SDK_ERROR_INTERNAL;
    } catch (...) {
        publish_diagnostic(report, diagnostic_utf8, diagnostic_capacity, "unknown SpinASM compiler failure");
        return FV1_SDK_ERROR_INTERNAL;
    }
}

fv1_sdk_result fv1_sdk_debug_begin_sample(fv1_sdk_engine* engine,
                                           float input_left,
                                           float input_right) {
    if (!engine || !engine->core) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    return map_result(fv1_debug_begin_sample(engine->core, input_left, input_right));
}

fv1_sdk_result fv1_sdk_debug_step_instruction_v1(fv1_sdk_engine* engine,
                                                  fv1_sdk_trace_v1* trace) {
    if (!engine || !engine->core || !trace) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    if (!valid_v1_header(trace->struct_size, trace->abi_version, sizeof(*trace))) {
        return FV1_SDK_ERROR_UNSUPPORTED;
    }
    fv1_trace core{};
    const fv1_result result = fv1_debug_step_instruction(engine->core, &core);
    if (result != FV1_OK) return map_result(result);
    const uint32_t size = trace->struct_size;
    const uint32_t version = trace->abi_version;
    std::memset(trace, 0, sizeof(*trace));
    trace->struct_size = size;
    trace->abi_version = version;
    trace->pc_before = core.pc_before;
    trace->pc_after = core.pc_after;
    trace->raw_instruction = core.raw_instruction;
    trace->opcode = core.opcode;
    trace->skipped = core.skipped;
    trace->sample_finished = core.sample_finished;
    trace->acc_before = core.acc_before;
    trace->acc_after = core.acc_after;
    trace->pacc_after = core.pacc_after;
    trace->lr_after = core.lr_after;
    trace->sample_index = core.sample_index;
    trace->instruction_index = core.instruction_index;
    return FV1_SDK_OK;
}

fv1_sdk_result fv1_sdk_debug_finish_sample(fv1_sdk_engine* engine,
                                            float* output_left,
                                            float* output_right) {
    if (!engine || !engine->core || !output_left || !output_right) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    return map_result(fv1_debug_finish_sample(engine->core, output_left, output_right));
}

fv1_sdk_result fv1_sdk_engine_get_state_digest_v1(const fv1_sdk_engine* engine,
                                                   fv1_sdk_state_digest_v1* digest) {
    if (!engine || !engine->core || !digest) return FV1_SDK_ERROR_INVALID_ARGUMENT;
    if (!valid_v1_header(digest->struct_size, digest->abi_version, sizeof(*digest))) {
        return FV1_SDK_ERROR_UNSUPPORTED;
    }
    fv1_state_digest core{};
    const fv1_result result = fv1_get_state_digest(engine->core, &core);
    if (result != FV1_OK) return map_result(result);
    const uint32_t size = digest->struct_size;
    const uint32_t version = digest->abi_version;
    std::memset(digest, 0, sizeof(*digest));
    digest->struct_size = size;
    digest->abi_version = version;
    digest->architectural_hash = core.architectural_hash;
    digest->delay_hash = core.delay_hash;
    digest->sample_counter = core.sample_counter;
    return FV1_SDK_OK;
}

const char* fv1_sdk_opcode_name(uint8_t opcode) { return fv1_opcode_name(opcode); }

const char* fv1_sdk_register_name(uint32_t register_index) {
    switch (register_index) {
        case FV1_SDK_REG_SIN0_RATE: return "SIN0_RATE";
        case FV1_SDK_REG_SIN0_RANGE: return "SIN0_RANGE";
        case FV1_SDK_REG_SIN1_RATE: return "SIN1_RATE";
        case FV1_SDK_REG_SIN1_RANGE: return "SIN1_RANGE";
        case FV1_SDK_REG_RMP0_RATE: return "RMP0_RATE";
        case FV1_SDK_REG_RMP0_RANGE: return "RMP0_RANGE";
        case FV1_SDK_REG_RMP1_RATE: return "RMP1_RATE";
        case FV1_SDK_REG_RMP1_RANGE: return "RMP1_RANGE";
        case FV1_SDK_REG_POT0: return "POT0";
        case FV1_SDK_REG_POT1: return "POT1";
        case FV1_SDK_REG_POT2: return "POT2";
        case FV1_SDK_REG_ADCL: return "ADCL";
        case FV1_SDK_REG_ADCR: return "ADCR";
        case FV1_SDK_REG_DACL: return "DACL";
        case FV1_SDK_REG_DACR: return "DACR";
        case FV1_SDK_REG_ADDR_PTR: return "ADDR_PTR";
        default:
            if (register_index >= FV1_SDK_REG0 && register_index <= FV1_SDK_REG31) {
                static const char* const names[32] = {
                    "REG0", "REG1", "REG2", "REG3", "REG4", "REG5", "REG6", "REG7",
                    "REG8", "REG9", "REG10", "REG11", "REG12", "REG13", "REG14", "REG15",
                    "REG16", "REG17", "REG18", "REG19", "REG20", "REG21", "REG22", "REG23",
                    "REG24", "REG25", "REG26", "REG27", "REG28", "REG29", "REG30", "REG31"
                };
                return names[register_index - FV1_SDK_REG0];
            }
            return "RESERVED";
    }
}

} // extern "C"
