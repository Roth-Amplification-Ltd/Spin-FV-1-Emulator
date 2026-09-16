#include <fv1/spinasm.hpp>

#include <cstdint>
#include <iostream>
#include <string>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
}

int main() {
    const std::string source = R"(
; exercise labels, MEM/EQU, aliases and expression forms
BUF MEM 32
GAIN EQU 0.5
start:
RDAX ADCL, 1.0
SOF GAIN, 0.0
WRA BUF, 0.0
RDA BUF#, 0.25
WRAX DACL, 0
LDAX ADCR
WRAX DACR, 0
SKP RUN, done
NOP
done:
)";

    const auto result = fv1::spinasm::compile(source);
    check(result.image.size() == 512, "compiler must emit exactly 512 bytes");
    check(result.instruction_count == 9, "instruction count");
    check(result.highest_delay_address == 32, "highest delay address");
    check(result.image[0] != 0 || result.image[1] != 0 || result.image[2] != 0 || result.image[3] != 0x11,
          "first instruction should not be NOP padding");
    check(result.image[508] == 0 && result.image[509] == 0 && result.image[510] == 0 && result.image[511] == 0x11,
          "last word should be NOP padding");

    try {
        (void)fv1::spinasm::compile("RDAX ADCL, 1.0\nNOPE REG0, 1.0\n");
        check(false, "invalid mnemonic must throw");
    } catch (const fv1::spinasm::CompileError& error) {
        check(error.line() == 2, "compile diagnostic must preserve line number");
        check(std::string(error.what()).find("unsupported mnemonic NOPE") != std::string::npos,
              "compile diagnostic should name invalid mnemonic");
    }

    try {
        (void)fv1::spinasm::compile("JMP done\ndone:\nNOP\n");
        check(false, "non-official JMP mnemonic must throw");
    } catch (const fv1::spinasm::CompileError& error) {
        check(error.line() == 1, "JMP rejection must preserve line number");
        check(std::string(error.what()).find("unsupported mnemonic JMP") != std::string::npos,
              "JMP rejection should match official SpinAsm syntax");
    }

    try {
        (void)fv1::spinasm::compile("SKP RUN, backwards\nbackwards:\nNOP\n");
    } catch (...) {
        check(false, "forward zero-offset label should compile");
    }

    // Official SpinAsm 1.1.31 truncates real fixed-point operands toward zero.
    // These words are oracle values captured from the real compiler.
    const auto quantized = fv1::spinasm::compile(
        "SOF 0.075, 0.004\n"
        "RDA 0, -0.22\n"
        "SOF -1.0, 0.999\n");

    const auto be_word = [&](std::size_t index) -> std::uint32_t {
        const std::size_t i = index * 4u;
        return (static_cast<std::uint32_t>(quantized.image[i]) << 24u) |
               (static_cast<std::uint32_t>(quantized.image[i + 1u]) << 16u) |
               (static_cast<std::uint32_t>(quantized.image[i + 2u]) << 8u) |
               static_cast<std::uint32_t>(quantized.image[i + 3u]);
    };

    check(be_word(0) == 0x04CC008Du,
          "SOF fixed-point literals must match official SpinAsm truncation");
    check(be_word(1) == 0xF2000000u,
          "negative S1.9 coefficient must truncate toward zero");
    check(be_word(2) == 0xC0007FCDu,
          "positive S.10 offset must truncate toward zero");

    if (failures == 0) std::cout << "fv1-spinasm native compiler tests passed\n";
    return failures == 0 ? 0 : 1;
}
