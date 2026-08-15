#pragma once

#include <fv1/fv1.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace fv1 {

struct ConformanceConfig {
    std::uint32_t samples = 256;
    std::uint64_t seed = UINT64_C(0x4656315c2026);
    fv1_delay_model delay_model = FV1_DELAY_REFERENCE_16;
    double virtual_sample_rate = 32768.0;
    bool compare_delay_memory = true;
};

struct ConformanceMismatch {
    std::uint64_t sample = 0;
    std::uint32_t instruction_index = 0;
    std::uint32_t program_counter = 0;
    std::string field;
    std::int64_t production_value = 0;
    std::int64_t reference_value = 0;
};

struct ConformanceReport {
    bool passed = false;
    std::uint64_t samples_compared = 0;
    std::uint64_t instructions_compared = 0;
    std::uint64_t seed = 0;
    std::array<std::uint64_t, 32> opcode_counts{};
    fv1_state_digest production_digest{};
    fv1_state_digest reference_digest{};
    ConformanceMismatch first_mismatch{};
    std::string failure;
};

/* Run the production engine and the intentionally independent reference model
   against identical deterministic input/POT vectors.  Comparison occurs after
   every executed instruction; complete state digests are compared at each
   sample boundary. */
ConformanceReport run_conformance(const std::uint8_t* program_image,
                                  std::size_t program_size,
                                  const ConformanceConfig& config = {});

std::string format_conformance_report(const ConformanceReport& report);

} // namespace fv1
