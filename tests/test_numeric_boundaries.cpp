#include <fv1/fv1.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {

[[noreturn]] void fail(const std::string& text) {
    std::cerr << "FAIL: " << text << "\n";
    std::exit(1);
}
void check(bool condition, const std::string& text) { if (!condition) fail(text); }

std::uint32_t enc_rdax(unsigned reg, double coeff) {
    const int c = static_cast<int>(std::llround(coeff * 16384.0));
    return (static_cast<std::uint32_t>(c) & 0xffffu) << 16u |
           ((reg & 0x3fu) << 5u) | 0x04u;
}
std::uint32_t enc_wrax(unsigned reg, double coeff) {
    const int c = static_cast<int>(std::llround(coeff * 16384.0));
    return (static_cast<std::uint32_t>(c) & 0xffffu) << 16u |
           ((reg & 0x3fu) << 5u) | 0x06u;
}
std::uint32_t enc_wra(unsigned address, double coeff) {
    const int c = static_cast<int>(std::llround(coeff * 512.0));
    return (static_cast<std::uint32_t>(c) & 0x7ffu) << 21u |
           ((address & 0x7fffu) << 5u) | 0x02u;
}
std::uint32_t enc_sof(double coeff, double offset) {
    const int c = static_cast<int>(std::llround(coeff * 16384.0));
    const int d = static_cast<int>(std::llround(offset * 1024.0));
    return (static_cast<std::uint32_t>(c) & 0xffffu) << 16u |
           (static_cast<std::uint32_t>(d) & 0x7ffu) << 5u | 0x0du;
}
std::uint32_t enc_xor(std::uint32_t mask) { return ((mask & 0xffffffu) << 8u) | 0x10u; }

std::array<std::uint32_t, FV1_PROGRAM_WORDS> make_program(std::initializer_list<std::uint32_t> code) {
    std::array<std::uint32_t, FV1_PROGRAM_WORDS> words{};
    words.fill(0x00000011u);
    std::size_t i = 0;
    for (const auto word : code) words[i++] = word;
    return words;
}

fv1_engine* load(const std::array<std::uint32_t, FV1_PROGRAM_WORDS>& words, fv1_delay_model model) {
    fv1_config cfg{32768.0, model};
    fv1_engine* e = fv1_create(&cfg);
    check(e != nullptr, "create");
    check(fv1_load_words(e, words.data(), words.size()) == FV1_OK, "load");
    return e;
}

void test_time_coordinates_and_digest() {
    auto words = make_program({enc_rdax(FV1_REG_ADCL, 1.0), enc_wrax(FV1_REG_DACL, 0.0)});
    fv1_engine* e = load(words, FV1_DELAY_FULL_24);
    check(fv1_debug_begin_sample(e, 0.25f, 0.0f) == FV1_OK, "begin");
    fv1_trace trace{};
    check(fv1_debug_step_instruction(e, &trace) == FV1_OK, "step");
    check(trace.sample_index == 0 && trace.instruction_index == 0, "first trace time coordinate");
    fv1_snapshot snap{};
    fv1_get_snapshot(e, &snap);
    check(snap.sample_counter == 0 && snap.instruction_counter == 1, "snapshot instruction coordinate");
    while (!trace.sample_finished) check(fv1_debug_step_instruction(e, &trace) == FV1_OK, "finish steps");
    fv1_get_snapshot(e, &snap);
    check(snap.sample_counter == 1 && snap.instruction_counter == 0, "sample counter increments exactly once");
    fv1_state_digest first{};
    check(fv1_get_state_digest(e, &first) == FV1_OK, "state digest");
    fv1_reset(e, 1);
    check(fv1_debug_begin_sample(e, 0.25f, 0.0f) == FV1_OK, "repeat begin");
    do { check(fv1_debug_step_instruction(e, &trace) == FV1_OK, "repeat step"); } while (!trace.sample_finished);
    fv1_state_digest second{};
    check(fv1_get_state_digest(e, &second) == FV1_OK, "repeat digest");
    check(first.architectural_hash == second.architectural_hash && first.delay_hash == second.delay_hash,
          "identical state/input is deterministic");
    fv1_destroy(e);
}

void test_delay_quantization_boundary() {
    auto words = make_program({enc_rdax(FV1_REG_ADCL, 1.0), enc_wra(0, 0.0)});
    fv1_engine* reduced = load(words, FV1_DELAY_REFERENCE_16);
    fv1_engine* full = load(words, FV1_DELAY_FULL_24);
    float out_l = 0.0f, out_r = 0.0f;
    constexpr float input = 0.1234567f;
    check(fv1_process_sample(reduced, input, 0.0f, &out_l, &out_r) == FV1_OK, "reduced process");
    check(fv1_process_sample(full, input, 0.0f, &out_l, &out_r) == FV1_OK, "full process");
    std::int32_t reduced_word = 0, full_word = 0;
    check(fv1_read_delay_word(reduced, 0, &reduced_word) == FV1_OK, "reduced read");
    check(fv1_read_delay_word(full, 0, &full_word) == FV1_OK, "full read");
    check((reduced_word & 0xff) == 0, "reference delay model clears lower eight datapath bits");
    check(reduced_word != full_word, "diagnostic full-24 model preserves extra precision");
    fv1_destroy(reduced);
    fv1_destroy(full);
}

void test_saturation_and_bitwise_edges() {
    auto positive = make_program({enc_rdax(FV1_REG_ADCL, 1.0), enc_sof(1.9999, 0.0), enc_wrax(FV1_REG_DACL, 0.0)});
    fv1_engine* e = load(positive, FV1_DELAY_FULL_24);
    float left = 0.0f, right = 0.0f;
    check(fv1_process_sample(e, 0.75f, 0.0f, &left, &right) == FV1_OK, "positive saturation");
    fv1_snapshot snap{}; fv1_get_snapshot(e, &snap);
    check(snap.regs[FV1_REG_DACL] == 0x7fffff, "positive saturation clamps at Q1.23 maximum");
    fv1_destroy(e);

    auto negative = make_program({enc_rdax(FV1_REG_ADCL, 1.0), enc_sof(1.9999, 0.0), enc_wrax(FV1_REG_DACL, 0.0)});
    e = load(negative, FV1_DELAY_FULL_24);
    check(fv1_process_sample(e, -0.75f, 0.0f, &left, &right) == FV1_OK, "negative saturation");
    fv1_get_snapshot(e, &snap);
    check(snap.regs[FV1_REG_DACL] == -0x800000, "negative saturation clamps at Q1.23 minimum");
    fv1_destroy(e);

    auto invert = make_program({enc_rdax(FV1_REG_ADCL, 1.0), enc_xor(0xffffffu), enc_wrax(FV1_REG_DACL, 0.0)});
    e = load(invert, FV1_DELAY_FULL_24);
    check(fv1_process_sample(e, 0.0f, 0.0f, &left, &right) == FV1_OK, "xor process");
    fv1_get_snapshot(e, &snap);
    check(snap.regs[FV1_REG_DACL] == -1, "XOR all ones is 24-bit ones-complement NOT");
    fv1_destroy(e);
}

void test_pot_code_endpoints() {
    auto words = make_program({0x00000205u, enc_wrax(FV1_REG_DACL, 0.0)}); // LDAX POT0
    fv1_engine* e = load(words, FV1_DELAY_FULL_24);
    float left = 0.0f, right = 0.0f;
    fv1_set_pots(e, 0.0f, 0.0f, 0.0f);
    check(fv1_process_sample(e, 0.0f, 0.0f, &left, &right) == FV1_OK, "pot zero");
    fv1_snapshot snap{}; fv1_get_snapshot(e, &snap);
    check(snap.regs[FV1_REG_POT0] == 0, "POT0 zero code");
    fv1_set_pots(e, 1.0f, 0.0f, 0.0f);
    check(fv1_process_sample(e, 0.0f, 0.0f, &left, &right) == FV1_OK, "pot max");
    fv1_get_snapshot(e, &snap);
    check(snap.regs[FV1_REG_POT0] == (511 << 14), "POT0 max is 511/512");
    fv1_destroy(e);
}

} // namespace

int main() {
    test_time_coordinates_and_digest();
    test_delay_quantization_boundary();
    test_saturation_and_bitwise_edges();
    test_pot_code_endpoints();
    std::cout << "Phase 5C numeric/time/state boundary tests: PASS\n";
    return 0;
}
