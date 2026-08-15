#include <fv1/conformance.hpp>
#include <fv1/reference_model.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

namespace fv1 {
namespace {

struct EngineDeleter {
    void operator()(fv1_engine* engine) const noexcept { fv1_destroy(engine); }
};
using EnginePtr = std::unique_ptr<fv1_engine, EngineDeleter>;

class DeterministicVectors {
public:
    explicit DeterministicVectors(std::uint64_t seed) : state_(seed ? seed : UINT64_C(0x4656315c2026)) {}

    std::uint64_t next() {
        std::uint64_t x = state_;
        x ^= x >> 12u;
        x ^= x << 25u;
        x ^= x >> 27u;
        state_ = x;
        return x * UINT64_C(2685821657736338717);
    }

    float audio() {
        const std::int16_t code = static_cast<std::int16_t>(next() >> 48u);
        return static_cast<float>(code) / 32768.0f;
    }

    float pot() {
        const std::uint32_t code = static_cast<std::uint32_t>(next() & 0x1ffu);
        return static_cast<float>(code) / 511.0f;
    }

private:
    std::uint64_t state_;
};

bool mismatch(ConformanceReport& report, std::uint64_t sample, std::uint32_t instruction,
              std::uint32_t pc, const char* field, std::int64_t production, std::int64_t reference) {
    report.first_mismatch.sample = sample;
    report.first_mismatch.instruction_index = instruction;
    report.first_mismatch.program_counter = pc;
    report.first_mismatch.field = field;
    report.first_mismatch.production_value = production;
    report.first_mismatch.reference_value = reference;
    report.failure = std::string("state mismatch: ") + field;
    return false;
}

bool compare_trace(const fv1_trace& production, const fv1_trace& reference,
                   ConformanceReport& report) {
#define FV1_CMP_TRACE(field) \
    do { if (production.field != reference.field) \
        return mismatch(report, production.sample_index, production.instruction_index, production.pc_before, \
                        #field, static_cast<std::int64_t>(production.field), \
                        static_cast<std::int64_t>(reference.field)); } while (false)
    FV1_CMP_TRACE(pc_before);
    FV1_CMP_TRACE(pc_after);
    FV1_CMP_TRACE(raw_instruction);
    FV1_CMP_TRACE(opcode);
    FV1_CMP_TRACE(acc_before);
    FV1_CMP_TRACE(acc_after);
    FV1_CMP_TRACE(pacc_after);
    FV1_CMP_TRACE(lr_after);
    FV1_CMP_TRACE(skipped);
    FV1_CMP_TRACE(sample_finished);
    FV1_CMP_TRACE(sample_index);
    FV1_CMP_TRACE(instruction_index);
#undef FV1_CMP_TRACE
    return true;
}

bool compare_snapshot(const fv1_snapshot& production, const fv1_snapshot& reference,
                      const fv1_trace& trace, ConformanceReport& report) {
    const std::uint64_t sample = trace.sample_index;
    const std::uint32_t instruction = trace.instruction_index;
    const std::uint32_t pc = trace.pc_before;
#define FV1_CMP_SNAPSHOT(field) \
    do { if (production.field != reference.field) \
        return mismatch(report, sample, instruction, pc, #field, \
                        static_cast<std::int64_t>(production.field), \
                        static_cast<std::int64_t>(reference.field)); } while (false)
    FV1_CMP_SNAPSHOT(acc);
    FV1_CMP_SNAPSHOT(pacc);
    FV1_CMP_SNAPSHOT(lr);
    FV1_CMP_SNAPSHOT(delay_pointer);
    FV1_CMP_SNAPSHOT(program_counter);
    FV1_CMP_SNAPSHOT(first_run);
    FV1_CMP_SNAPSHOT(debug_sample_active);
    FV1_CMP_SNAPSHOT(sample_counter);
    FV1_CMP_SNAPSHOT(instruction_counter);
#undef FV1_CMP_SNAPSHOT

    for (std::size_t i = 0; i < FV1_REGISTER_COUNT; ++i) {
        if (production.regs[i] != reference.regs[i]) {
            std::ostringstream name;
            name << "regs[" << i << "]";
            return mismatch(report, sample, instruction, pc, name.str().c_str(),
                            production.regs[i], reference.regs[i]);
        }
    }
    for (std::size_t i = 0; i < 2; ++i) {
        if (production.sin_lfo[i] != reference.sin_lfo[i]) {
            std::ostringstream name; name << "sin_lfo[" << i << "]";
            return mismatch(report, sample, instruction, pc, name.str().c_str(),
                            production.sin_lfo[i], reference.sin_lfo[i]);
        }
        if (production.cos_lfo[i] != reference.cos_lfo[i]) {
            std::ostringstream name; name << "cos_lfo[" << i << "]";
            return mismatch(report, sample, instruction, pc, name.str().c_str(),
                            production.cos_lfo[i], reference.cos_lfo[i]);
        }
        if (production.ramp_lfo[i] != reference.ramp_lfo[i]) {
            std::ostringstream name; name << "ramp_lfo[" << i << "]";
            return mismatch(report, sample, instruction, pc, name.str().c_str(),
                            production.ramp_lfo[i], reference.ramp_lfo[i]);
        }
    }
    return true;
}

} // namespace

ConformanceReport run_conformance(const std::uint8_t* program_image,
                                  std::size_t program_size,
                                  const ConformanceConfig& config) {
    ConformanceReport report{};
    report.seed = config.seed;
    if (!program_image || program_size != FV1_PROGRAM_BYTES) {
        report.failure = "conformance requires exactly one 512-byte FV-1 program image";
        return report;
    }

    fv1_config engine_config{config.virtual_sample_rate, config.delay_model};
    EnginePtr production(fv1_create(&engine_config));
    if (!production) {
        report.failure = "could not allocate production engine";
        return report;
    }
    ReferenceModel reference(engine_config);

    if (fv1_load_bytes(production.get(), program_image, program_size) != FV1_OK ||
        reference.load_bytes(program_image, program_size) != FV1_OK) {
        report.failure = "program image rejected by production or reference model";
        return report;
    }

    DeterministicVectors vectors(config.seed);
    for (std::uint32_t sample = 0; sample < config.samples; ++sample) {
        const float in_l = vectors.audio();
        const float in_r = vectors.audio();
        const float pot0 = vectors.pot();
        const float pot1 = vectors.pot();
        const float pot2 = vectors.pot();
        fv1_set_pots(production.get(), pot0, pot1, pot2);
        reference.set_pots(pot0, pot1, pot2);

        fv1_result prod_result = fv1_debug_begin_sample(production.get(), in_l, in_r);
        fv1_result ref_result = reference.begin_sample(in_l, in_r);
        if (prod_result != ref_result || prod_result != FV1_OK) {
            report.failure = "begin_sample result mismatch";
            return report;
        }

        for (;;) {
            fv1_trace prod_trace{};
            fv1_trace ref_trace{};
            prod_result = fv1_debug_step_instruction(production.get(), &prod_trace);
            ref_result = reference.step_instruction(&ref_trace);
            if (prod_result != ref_result || prod_result != FV1_OK) {
                report.failure = "instruction-step result mismatch";
                return report;
            }
            ++report.instructions_compared;
            if (prod_trace.opcode < report.opcode_counts.size()) ++report.opcode_counts[prod_trace.opcode];
            if (!compare_trace(prod_trace, ref_trace, report)) return report;

            fv1_snapshot prod_snapshot{};
            fv1_snapshot ref_snapshot{};
            fv1_get_snapshot(production.get(), &prod_snapshot);
            reference.get_snapshot(&ref_snapshot);
            if (!compare_snapshot(prod_snapshot, ref_snapshot, prod_trace, report)) return report;

            if (prod_trace.sample_finished) break;
            if (report.instructions_compared >
                static_cast<std::uint64_t>(config.samples) * FV1_PROGRAM_WORDS) {
                report.failure = "instruction comparison safety limit exceeded";
                return report;
            }
        }

        if (fv1_get_state_digest(production.get(), &report.production_digest) != FV1_OK ||
            reference.get_state_digest(&report.reference_digest) != FV1_OK) {
            report.failure = "could not obtain state digest";
            return report;
        }
        if (report.production_digest.architectural_hash != report.reference_digest.architectural_hash) {
            mismatch(report, sample, 0, 0, "architectural_hash",
                     static_cast<std::int64_t>(report.production_digest.architectural_hash),
                     static_cast<std::int64_t>(report.reference_digest.architectural_hash));
            return report;
        }
        if (config.compare_delay_memory &&
            report.production_digest.delay_hash != report.reference_digest.delay_hash) {
            mismatch(report, sample, 0, 0, "delay_hash",
                     static_cast<std::int64_t>(report.production_digest.delay_hash),
                     static_cast<std::int64_t>(report.reference_digest.delay_hash));
            return report;
        }

        float prod_l = 0.0f, prod_r = 0.0f, ref_l = 0.0f, ref_r = 0.0f;
        prod_result = fv1_debug_finish_sample(production.get(), &prod_l, &prod_r);
        ref_result = reference.finish_sample(&ref_l, &ref_r);
        if (prod_result != ref_result || prod_result != FV1_OK ||
            std::memcmp(&prod_l, &ref_l, sizeof(float)) != 0 ||
            std::memcmp(&prod_r, &ref_r, sizeof(float)) != 0) {
            report.failure = "sample output mismatch";
            return report;
        }
        report.samples_compared = static_cast<std::uint64_t>(sample) + 1u;
    }

    report.passed = true;
    return report;
}

std::string format_conformance_report(const ConformanceReport& report) {
    std::ostringstream out;
    out << "FV-1 production/reference conformance\n"
        << "  result:                 " << (report.passed ? "PASS" : "FAIL") << "\n"
        << "  seed:                   0x" << std::hex << report.seed << std::dec << "\n"
        << "  samples compared:       " << report.samples_compared << "\n"
        << "  instructions compared:  " << report.instructions_compared << "\n";
    out << "  opcodes observed:        ";
    bool first_opcode = true;
    for (std::size_t opcode = 0; opcode < report.opcode_counts.size(); ++opcode) {
        if (report.opcode_counts[opcode] == 0) continue;
        if (!first_opcode) out << ", ";
        first_opcode = false;
        out << fv1_opcode_name(static_cast<std::uint8_t>(opcode)) << "=" << report.opcode_counts[opcode];
    }
    if (first_opcode) out << "none";
    out << "\n";
    if (report.production_digest.sample_counter || report.reference_digest.sample_counter) {
        out << "  architectural hash:     0x" << std::hex << report.production_digest.architectural_hash
            << " / 0x" << report.reference_digest.architectural_hash << "\n"
            << "  delay-memory hash:      0x" << report.production_digest.delay_hash
            << " / 0x" << report.reference_digest.delay_hash << std::dec << "\n";
    }
    if (!report.passed) {
        out << "  failure:                " << report.failure << "\n";
        if (!report.first_mismatch.field.empty()) {
            out << "  first divergence:       sample " << report.first_mismatch.sample
                << ", executed instruction " << report.first_mismatch.instruction_index
                << ", PC " << report.first_mismatch.program_counter << "\n"
                << "  field:                  " << report.first_mismatch.field << "\n"
                << "  production/reference:   " << report.first_mismatch.production_value
                << " / " << report.first_mismatch.reference_value << "\n";
        }
    }
    return out.str();
}

} // namespace fv1
