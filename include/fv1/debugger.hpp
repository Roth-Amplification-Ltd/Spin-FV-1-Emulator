#pragma once

#include <fv1/fv1.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_set>

namespace fv1 {

struct DebugStep {
    fv1_trace trace{};
    fv1_snapshot snapshot{};
    bool breakpoint_hit{};
    bool sample_finished{};
    std::uint64_t sample_index{};
};

/* GUI-independent offline debugger for one FV-1 instance.

   Realtime processing and instruction stepping are intentionally separate.
   A Debugger owns a private chip instance, so frontends can single-step,
   inspect registers and stop at breakpoints without ever racing the realtime
   AudioHost/Runtime instance. */
class Debugger {
public:
    explicit Debugger(double virtual_sample_rate = 32768.0,
                      fv1_delay_model delay_model = FV1_DELAY_REFERENCE_16);
    ~Debugger();
    Debugger(const Debugger&) = delete;
    Debugger& operator=(const Debugger&) = delete;
    Debugger(Debugger&&) noexcept;
    Debugger& operator=(Debugger&&) noexcept;

    bool load_program(std::span<const std::uint8_t> bytes);
    void reset(bool clear_delay_ram = true);
    void set_pots(float pot0, float pot1, float pot2);
    void set_input(float left, float right) noexcept;

    bool begin_sample();
    bool step_instruction(DebugStep& step);
    bool step_sample(DebugStep& final_step, std::size_t max_instructions = FV1_PROGRAM_WORDS + 1u);
    bool continue_until_breakpoint(const std::unordered_set<std::uint32_t>& breakpoint_pcs,
                                   DebugStep& final_step,
                                   std::size_t max_instructions = FV1_PROGRAM_WORDS + 1u);

    fv1_snapshot snapshot() const noexcept;
    DebugStep last_step() const noexcept;
    bool read_delay_word(std::uint32_t address, std::int32_t& value) const noexcept;
    bool sample_active() const noexcept;
    std::uint64_t sample_index() const noexcept;
    float last_output_left() const noexcept;
    float last_output_right() const noexcept;
    fv1_engine* engine() noexcept;
    const fv1_engine* engine() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fv1
