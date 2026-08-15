#include <fv1/conformance.hpp>

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (!data || size < FV1_PROGRAM_BYTES) return 0;

    // The production loader deliberately rejects undefined opcode values.  Do
    // that cheap filter here so the fuzzer spends its time exploring programs
    // that both execution models are expected to understand.
    for (std::size_t word = 0; word < FV1_PROGRAM_WORDS; ++word) {
        const std::size_t offset = word * 4u;
        const std::uint32_t raw = (static_cast<std::uint32_t>(data[offset]) << 24u) |
                                  (static_cast<std::uint32_t>(data[offset + 1u]) << 16u) |
                                  (static_cast<std::uint32_t>(data[offset + 2u]) << 8u) |
                                  static_cast<std::uint32_t>(data[offset + 3u]);
        if ((raw & 0x1fu) > 0x14u) return 0;
    }

    std::uint64_t seed = UINT64_C(0x4656315c2026);
    for (std::size_t i = FV1_PROGRAM_BYTES; i < size && i < FV1_PROGRAM_BYTES + 8u; ++i)
        seed = (seed << 8u) ^ data[i];

    fv1::ConformanceConfig cfg;
    cfg.samples = 2;
    cfg.seed = seed;
    cfg.compare_delay_memory = true;
    cfg.delay_model = (size > FV1_PROGRAM_BYTES + 8u && (data[FV1_PROGRAM_BYTES + 8u] & 1u))
        ? FV1_DELAY_FULL_24 : FV1_DELAY_REFERENCE_16;

    const auto report = fv1::run_conformance(data, FV1_PROGRAM_BYTES, cfg);
    if (!report.passed) __builtin_trap();
    return 0;
}
