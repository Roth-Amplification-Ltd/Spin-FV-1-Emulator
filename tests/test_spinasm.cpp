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
        (void)fv1::spinasm::compile("SKP RUN, backwards\nbackwards:\nNOP\n");
    } catch (...) {
        check(false, "forward zero-offset label should compile");
    }

    if (failures == 0) std::cout << "fv1-spinasm native compiler tests passed\n";
    return failures == 0 ? 0 : 1;
}
