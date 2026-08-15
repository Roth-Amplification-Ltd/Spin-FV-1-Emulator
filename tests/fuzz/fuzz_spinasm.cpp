#include <fv1/spinasm.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    // Keep one fuzz iteration bounded while still allowing sources far larger
    // than a legal 128-instruction FV-1 program. The parser must reject bad or
    // oversized inputs cleanly rather than crash or invoke undefined behavior.
    if (!data || size > 64u * 1024u) return 0;

    const auto* chars = reinterpret_cast<const char*>(data);
    try {
        (void)fv1::spinasm::compile(std::string_view(chars, size));
    } catch (const fv1::spinasm::CompileError&) {
        // Expected outcome for arbitrary/malformed input.
    }
    return 0;
}
