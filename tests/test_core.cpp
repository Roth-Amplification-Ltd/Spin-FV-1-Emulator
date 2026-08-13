#include <fv1/fv1.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

[[noreturn]] void fail(const std::string& msg) {
    std::cerr << "FAIL: " << msg << "\n";
    std::exit(1);
}

void check(bool cond, const std::string& msg) {
    if (!cond) fail(msg);
}

bool near(float a, float b, float eps = 2.0e-4f) {
    return std::fabs(a - b) <= eps;
}

uint32_t enc_rdax(unsigned reg, double coeff) {
    const int c = static_cast<int>(std::llround(coeff * 16384.0));
    return (static_cast<uint32_t>(c) & 0xffffu) << 16 |
           ((reg & 0x3fu) << 5) | 0x04u;
}

uint32_t enc_ldax(unsigned reg) {
    return ((reg & 0x3fu) << 5) | 0x05u;
}

uint32_t enc_wrax(unsigned reg, double coeff) {
    const int c = static_cast<int>(std::llround(coeff * 16384.0));
    return (static_cast<uint32_t>(c) & 0xffffu) << 16 |
           ((reg & 0x3fu) << 5) | 0x06u;
}

uint32_t enc_wra(unsigned addr, double coeff) {
    const int c = static_cast<int>(std::llround(coeff * 512.0));
    return (static_cast<uint32_t>(c) & 0x7ffu) << 21 |
           ((addr & 0x7fffu) << 5) | 0x02u;
}

uint32_t enc_rda(unsigned addr, double coeff) {
    const int c = static_cast<int>(std::llround(coeff * 512.0));
    return (static_cast<uint32_t>(c) & 0x7ffu) << 21 |
           ((addr & 0x7fffu) << 5) | 0x00u;
}

uint32_t enc_skp(unsigned cond, unsigned count) {
    return ((cond & 0x1fu) << 27) | ((count & 0x3fu) << 21) | 0x11u;
}

std::array<uint32_t, FV1_PROGRAM_WORDS> program(std::initializer_list<uint32_t> code) {
    std::array<uint32_t, FV1_PROGRAM_WORDS> p{};
    p.fill(0x00000011u);
    size_t i = 0;
    for (auto w : code) p[i++] = w;
    return p;
}

fv1_engine* engine_with(const std::array<uint32_t, FV1_PROGRAM_WORDS>& p,
                        fv1_delay_model delay = FV1_DELAY_FULL_24) {
    fv1_config cfg{32768.0, delay};
    fv1_engine* e = fv1_create(&cfg);
    check(e != nullptr, "fv1_create");
    check(fv1_load_words(e, p.data(), p.size()) == FV1_OK, "fv1_load_words");
    return e;
}

void test_passthrough() {
    auto p = program({
        enc_rdax(FV1_REG_ADCL, 1.0), enc_wrax(FV1_REG_DACL, 0.0),
        enc_rdax(FV1_REG_ADCR, 1.0), enc_wrax(FV1_REG_DACR, 0.0)
    });
    auto* e = engine_with(p);
    float l = 0, r = 0;
    check(fv1_process_sample(e, 0.25f, -0.5f, &l, &r) == FV1_OK, "process passthrough");
    check(near(l, 0.25f), "left passthrough");
    check(near(r, -0.5f), "right passthrough");
    fv1_destroy(e);
}

void test_pot_quantization() {
    auto p = program({enc_ldax(FV1_REG_POT0), enc_wrax(FV1_REG_DACL, 0.0)});
    auto* e = engine_with(p);
    fv1_set_pots(e, 1.0f, 0.0f, 0.0f);
    float l = 0, r = 0;
    check(fv1_process_sample(e, 0, 0, &l, &r) == FV1_OK, "process pot");
    check(near(l, 511.0f / 512.0f, 1.0e-5f), "9-bit POT max");
    fv1_destroy(e);
}

void test_delay_one_sample() {
    // Write current input at relative 0, then read relative +1. Since the FV-1
    // decrements its physical delay pointer after each sample, +1 on the next
    // sample addresses the previous write location.
    auto p = program({
        enc_ldax(FV1_REG_ADCL), enc_wra(0, 0.0),
        enc_rda(1, 1.0), enc_wrax(FV1_REG_DACL, 0.0)
    });
    auto* e = engine_with(p);
    float l = 0, r = 0;
    check(fv1_process_sample(e, 0.4f, 0, &l, &r) == FV1_OK, "delay sample 1");
    check(near(l, 0.0f), "delay initially zero");
    check(fv1_process_sample(e, -0.2f, 0, &l, &r) == FV1_OK, "delay sample 2");
    check(near(l, 0.4f), "one-sample delay");
    fv1_destroy(e);
}

void test_run_skip() {
    // First sample: SKP RUN is false, so DACL gets +0.5.
    // Later samples: RUN is true, skipping the +0.5 load and writing zero.
    auto p = program({
        enc_skp(0x10, 1),
        enc_rdax(FV1_REG_ADCL, 1.0),
        enc_wrax(FV1_REG_DACL, 0.0)
    });
    auto* e = engine_with(p);
    float l = 0, r = 0;
    check(fv1_process_sample(e, 0.5f, 0, &l, &r) == FV1_OK, "run skip first");
    check(near(l, 0.5f), "RUN first sample executes setup path");
    check(fv1_process_sample(e, 0.5f, 0, &l, &r) == FV1_OK, "run skip later");
    // DACL is not cleared by the FV-1 between samples; skipping a DAC write
    // therefore leaves its previous value. What matters here is the branch.
    fv1_snapshot s{};
    fv1_get_snapshot(e, &s);
    check(s.first_run == 0, "first_run clears after sample");
    fv1_destroy(e);
}


void test_unconditional_jump() {
    auto p = program({
        enc_skp(0x00, 1), // JMP over the first RDAX
        enc_rdax(FV1_REG_ADCL, 1.0),
        enc_rdax(FV1_REG_ADCR, 1.0),
        enc_wrax(FV1_REG_DACL, 0.0)
    });
    auto* e = engine_with(p);
    float l = 0, r = 0;
    check(fv1_process_sample(e, 0.75f, -0.25f, &l, &r) == FV1_OK, "unconditional jump process");
    check(near(l, -0.25f), "SKP condition 0 behaves as JMP");
    fv1_destroy(e);
}

void test_debugger() {
    auto p = program({enc_rdax(FV1_REG_ADCL, 1.0), enc_wrax(FV1_REG_DACL, 0.0)});
    auto* e = engine_with(p);
    check(fv1_debug_begin_sample(e, 0.125f, 0.0f) == FV1_OK, "debug begin");
    fv1_trace t{};
    check(fv1_debug_step_instruction(e, &t) == FV1_OK, "debug step 1");
    check(t.pc_before == 0 && t.opcode == 0x04, "debug trace pc/opcode");
    check(t.acc_after != 0, "debug accumulator changed");
    unsigned steps = 1;
    while (!t.sample_finished && steps < 256) {
        check(fv1_debug_step_instruction(e, &t) == FV1_OK, "debug continue");
        ++steps;
    }
    check(t.sample_finished != 0, "debug sample completes");
    float l = 0, r = 0;
    check(fv1_debug_finish_sample(e, &l, &r) == FV1_OK, "debug finish");
    check(near(l, 0.125f), "debug output");
    fv1_destroy(e);
}

void test_resource_report() {
    auto p = program({
        enc_ldax(FV1_REG_POT0),
        enc_wrax(FV1_REG0, 0.0),
        enc_skp(0x01, 1),
        enc_rda(123, 0.5),
        enc_wra(456, 0.0)
    });
    auto* e = engine_with(p);
    fv1_resource_report r{};
    check(fv1_analyze_program(e, &r) == FV1_OK, "analyze");
    check(r.used_instructions == 5, "used instruction count");
    check(r.general_registers_used == 1, "register resource count");
    check(r.pots_used == 1, "pot resource count");
    check(r.static_delay_reads == 1 && r.static_delay_writes == 1, "delay op counts");
    check(r.highest_static_delay_address == 456, "highest static delay address");
    check(r.skip_instructions == 1, "skip count");
    fv1_destroy(e);
}

} // namespace

int main() {
    test_passthrough();
    test_pot_quantization();
    test_delay_one_sample();
    test_run_skip();
    test_unconditional_jump();
    test_debugger();
    test_resource_report();
    std::cout << "fv1-core Phase-1 tests: PASS\n";
    return 0;
}
