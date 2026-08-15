#include <fv1/conformance.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>

namespace {

[[noreturn]] void fail(const std::string& text) {
    std::cerr << "FAIL: " << text << "\n";
    std::exit(1);
}

void check(bool condition, const std::string& text) {
    if (!condition) fail(text);
}

std::uint32_t signed_bits(std::int32_t value, unsigned bits) {
    return static_cast<std::uint32_t>(value) & ((1u << bits) - 1u);
}

std::array<std::uint8_t, FV1_PROGRAM_BYTES> random_program(std::mt19937_64& rng) {
    std::array<std::uint8_t, FV1_PROGRAM_BYTES> image{};
    for (std::size_t pc = 0; pc < FV1_PROGRAM_WORDS; ++pc) {
        const std::uint8_t opcode = static_cast<std::uint8_t>(rng() % 0x15u);
        std::uint32_t word = opcode;
        switch (opcode) {
            case 0x00: // RDA
            case 0x02: // WRA
            case 0x03: // WRAP
                word |= (static_cast<std::uint32_t>(rng() & 0x7fffu) << 5u);
                word |= (signed_bits(static_cast<std::int32_t>(rng() & 0x7ffu), 11) << 21u);
                break;
            case 0x01: // RMPA
                word |= (signed_bits(static_cast<std::int32_t>(rng() & 0x7ffu), 11) << 21u);
                break;
            case 0x04: case 0x05: case 0x06: case 0x07: case 0x08: case 0x09:
                word |= (static_cast<std::uint32_t>(rng() & 0x3fu) << 5u);
                word |= (static_cast<std::uint32_t>(rng() & 0xffffu) << 16u);
                break;
            case 0x0a:
                word |= (static_cast<std::uint32_t>(rng() & 0x3fu) << 5u);
                break;
            case 0x0b: case 0x0c: case 0x0d:
                word |= (static_cast<std::uint32_t>(rng() & 0x7ffu) << 5u);
                word |= (static_cast<std::uint32_t>(rng() & 0xffffu) << 16u);
                break;
            case 0x0e: case 0x0f: case 0x10:
                word |= (static_cast<std::uint32_t>(rng() & 0xffffffu) << 8u);
                break;
            case 0x11: { // forward-only SKP; bound count to remaining words
                const std::uint32_t remaining = static_cast<std::uint32_t>(FV1_PROGRAM_WORDS - pc - 1u);
                const std::uint32_t count = remaining ? static_cast<std::uint32_t>(rng() % (std::min(remaining, 63u) + 1u)) : 0u;
                word |= (static_cast<std::uint32_t>(rng() & 0x1fu) << 27u);
                word |= count << 21u;
                break;
            }
            case 0x12:
                if ((rng() & 1u) == 0u) {
                    word |= (static_cast<std::uint32_t>(rng() & 1u) << 29u);
                    word |= (static_cast<std::uint32_t>(rng() & 0x1ffu) << 20u);
                    word |= (static_cast<std::uint32_t>(rng() & 0x7fffu) << 5u);
                } else {
                    word |= 0x40000000u;
                    word |= (static_cast<std::uint32_t>(rng() & 1u) << 29u);
                    word |= (static_cast<std::uint32_t>(rng() & 0xffffu) << 13u);
                    word |= (static_cast<std::uint32_t>(rng() & 3u) << 5u);
                }
                break;
            case 0x13:
                word |= (static_cast<std::uint32_t>(rng() & 1u) << 6u);
                break;
            case 0x14: {
                const std::uint32_t type_choices[3]{0u, 2u, 3u};
                const std::uint32_t type = type_choices[rng() % 3u];
                word |= type << 30u;
                word |= (static_cast<std::uint32_t>(rng() & 0x3fu) << 24u);
                word |= (static_cast<std::uint32_t>(rng() & 3u) << 21u);
                word |= (static_cast<std::uint32_t>(rng() & 0xffffu) << 5u);
                break;
            }
            default:
                break;
        }
        const std::size_t offset = pc * 4u;
        image[offset] = static_cast<std::uint8_t>(word >> 24u);
        image[offset + 1u] = static_cast<std::uint8_t>(word >> 16u);
        image[offset + 2u] = static_cast<std::uint8_t>(word >> 8u);
        image[offset + 3u] = static_cast<std::uint8_t>(word);
    }
    return image;
}

} // namespace

int main() {
    std::mt19937_64 generator(UINT64_C(0x5c4656312026));
    std::array<std::uint64_t, 32> aggregate_opcode_counts{};
    for (unsigned program_index = 0; program_index < 48; ++program_index) {
        const auto image = random_program(generator);
        fv1::ConformanceConfig cfg;
        cfg.samples = 6;
        cfg.seed = generator();
        cfg.delay_model = (program_index & 1u) ? FV1_DELAY_REFERENCE_16 : FV1_DELAY_FULL_24;
        const auto report = fv1::run_conformance(image.data(), image.size(), cfg);
        if (!report.passed) {
            std::cerr << fv1::format_conformance_report(report);
            fail("random differential program " + std::to_string(program_index));
        }
        check(report.samples_compared == cfg.samples, "sample count");
        check(report.instructions_compared > 0, "instruction comparison count");
        for (std::size_t opcode = 0; opcode < aggregate_opcode_counts.size(); ++opcode)
            aggregate_opcode_counts[opcode] += report.opcode_counts[opcode];
    }
    for (std::uint8_t opcode = 0; opcode <= 0x14u; ++opcode) {
        check(aggregate_opcode_counts[opcode] > 0,
              "randomized conformance corpus executed opcode " + std::to_string(opcode));
    }
    std::cout << "Phase 5C randomized production/reference differential tests: PASS\n";
    return 0;
}
