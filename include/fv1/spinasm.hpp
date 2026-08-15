#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fv1::spinasm {

inline constexpr std::size_t program_bytes = 512;
inline constexpr std::size_t program_words = 128;

struct CompileResult {
    std::array<std::uint8_t, program_bytes> image{};
    std::uint32_t instruction_count{};
    std::uint32_t highest_delay_address{};
};

class CompileError final : public std::runtime_error {
public:
    CompileError(std::uint32_t line, std::string message)
        : std::runtime_error(std::move(message)), line_(line) {}

    std::uint32_t line() const noexcept { return line_; }

private:
    std::uint32_t line_{};
};

// Native SpinASM compiler used by all application frontends and exposed to
// external programs through the Phase-6 SDK C ABI. This intentionally mirrors
// the project's historical Python assembler semantics so existing programs
// remain byte-for-byte reproducible without a Python runtime dependency.
CompileResult compile(std::string_view source);

} // namespace fv1::spinasm
