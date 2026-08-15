#include <fv1/fv1.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
[[noreturn]] void fail(const std::string& text) { std::cerr << "FAIL: " << text << "\n"; std::exit(1); }
void check(bool ok, const std::string& text) { if (!ok) fail(text); }

std::uint32_t q14(double v) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(std::llround(v * 16384.0))) & 0xffffu; }
std::uint32_t q10(double v) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(std::llround(v * 1024.0))) & 0x7ffu; }
std::uint32_t enc_rdax(unsigned reg, double c) { return (q14(c) << 16u) | ((reg & 0x3fu) << 5u) | 0x04u; }
std::uint32_t enc_rdfx(unsigned reg, double c) { return (q14(c) << 16u) | ((reg & 0x3fu) << 5u) | 0x05u; }
std::uint32_t enc_wrax(unsigned reg, double c) { return (q14(c) << 16u) | ((reg & 0x3fu) << 5u) | 0x06u; }
std::uint32_t enc_mulx(unsigned reg) { return ((reg & 0x3fu) << 5u) | 0x0au; }
std::uint32_t enc_log(double c, double d) { return (q14(c) << 16u) | (q10(d) << 5u) | 0x0bu; }
std::uint32_t enc_exp(double c, double d) { return (q14(c) << 16u) | (q10(d) << 5u) | 0x0cu; }
constexpr std::uint32_t enc_clr() { return 0x0eu; }

std::array<std::uint32_t, FV1_PROGRAM_WORDS> program(std::initializer_list<std::uint32_t> code) {
    std::array<std::uint32_t, FV1_PROGRAM_WORDS> words{};
    words.fill(0x11u); // canonical zero-count NOP
    std::size_t n = 0;
    for (auto word : code) words[n++] = word;
    return words;
}

float run_left(const std::array<std::uint32_t, FV1_PROGRAM_WORDS>& words, float l, float r) {
    fv1_config cfg{32768.0, FV1_DELAY_FULL_24};
    fv1_engine* e = fv1_create(&cfg);
    check(e != nullptr, "engine allocation");
    check(fv1_load_words(e, words.data(), words.size()) == FV1_OK, "program load");
    float out_l = 0.0f, out_r = 0.0f;
    check(fv1_process_sample(e, l, r, &out_l, &out_r) == FV1_OK, "sample execution");
    fv1_destroy(e);
    return out_l;
}

void near(float actual, double expected, double tol, const char* label) {
    if (std::abs(static_cast<double>(actual) - expected) > tol) {
        std::cerr << label << ": actual=" << actual << " expected=" << expected << "\n";
        fail(label);
    }
}

void test_rdax_and_register_coefficient() {
    const auto words = program({
        enc_rdax(FV1_REG_ADCL, 1.0),
        enc_wrax(FV1_REG0, 0.0),
        enc_clr(),
        enc_rdax(FV1_REG0, 0.5),
        enc_wrax(FV1_REG_DACL, 0.0),
    });
    near(run_left(words, 0.5f, 0.0f), 0.25, 2.0 / (1u << 23), "RDAX Q coefficient semantics");
}

void test_rdfx_documented_filter_form() {
    const auto words = program({
        enc_rdax(FV1_REG_ADCL, 1.0),       // 0.25
        enc_wrax(FV1_REG0, 0.0),           // reg0=0.25, ACC=0
        enc_rdax(FV1_REG_ADCR, 1.0),       // ACC=0.75
        enc_rdfx(FV1_REG0, 0.5),           // reg + (ACC-reg)*0.5 = 0.5
        enc_wrax(FV1_REG_DACL, 0.0),
    });
    near(run_left(words, 0.25f, 0.75f), 0.5, 3.0 / (1u << 23), "RDFX filter form");
}

void test_mulx_fractional_product() {
    const auto words = program({
        enc_rdax(FV1_REG_ADCL, 1.0),
        enc_wrax(FV1_REG0, 0.0),           // reg0=+0.5
        enc_rdax(FV1_REG_ADCR, 1.0),       // ACC=-0.5
        enc_mulx(FV1_REG0),                // -0.25
        enc_wrax(FV1_REG_DACL, 0.0),
    });
    near(run_left(words, 0.5f, -0.5f), -0.25, 3.0 / (1u << 23), "MULX fractional product");
}

void test_log_exp_documented_scaling() {
    const auto words = program({
        enc_rdax(FV1_REG_ADCL, 1.0),       // 0.25 = 2^-2
        enc_log(1.0, 0.0),                 // log2(0.25)/16 = -0.125
        enc_exp(1.0, 0.0),                 // 2^(-0.125*16) = 0.25
        enc_wrax(FV1_REG_DACL, 0.0),
    });
    near(run_left(words, 0.25f, 0.0f), 0.25, 8.0 / (1u << 23), "LOG/EXP /16 reciprocal scaling");
}
}

int main() {
    test_rdax_and_register_coefficient();
    test_rdfx_documented_filter_form();
    test_mulx_fractional_product();
    test_log_exp_documented_scaling();
    std::cout << "Phase 5C specification-derived instruction contract tests: PASS\n";
    return 0;
}
