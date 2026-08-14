#include <fv1/debugger.hpp>

#include <algorithm>
#include <utility>

namespace fv1 {

class Debugger::Impl {
public:
    Impl(double rate, fv1_delay_model delay_model) {
        fv1_config cfg{};
        cfg.virtual_sample_rate = rate > 0.0 ? rate : 32768.0;
        cfg.delay_model = delay_model;
        engine = fv1_create(&cfg);
    }

    ~Impl() { fv1_destroy(engine); }

    fv1_engine* engine{};
    float input_l{};
    float input_r{};
    float output_l{};
    float output_r{};
    std::uint64_t sample_index{};
    DebugStep last_step{};
};

Debugger::Debugger(double rate, fv1_delay_model model) : impl_(std::make_unique<Impl>(rate, model)) {}
Debugger::~Debugger() = default;
Debugger::Debugger(Debugger&&) noexcept = default;
Debugger& Debugger::operator=(Debugger&&) noexcept = default;

bool Debugger::load_program(std::span<const std::uint8_t> bytes) {
    if (!impl_ || !impl_->engine || bytes.size() != FV1_PROGRAM_BYTES) return false;
    const auto result = fv1_load_bytes(impl_->engine, bytes.data(), bytes.size());
    if (result == FV1_OK) fv1_reset(impl_->engine, 1);
    impl_->sample_index = 0;
    impl_->output_l = 0.0f;
    impl_->output_r = 0.0f;
    impl_->last_step = {};
    return result == FV1_OK;
}

void Debugger::reset(bool clear_delay_ram) {
    if (!impl_ || !impl_->engine) return;
    fv1_reset(impl_->engine, clear_delay_ram ? 1 : 0);
    impl_->sample_index = 0;
    impl_->output_l = 0.0f;
    impl_->output_r = 0.0f;
    impl_->last_step = {};
}

void Debugger::set_pots(float a, float b, float c) {
    if (impl_ && impl_->engine) fv1_set_pots(impl_->engine, a, b, c);
}

void Debugger::set_input(float left, float right) noexcept {
    if (!impl_) return;
    impl_->input_l = std::clamp(left, -1.0f, 1.0f);
    impl_->input_r = std::clamp(right, -1.0f, 1.0f);
}

bool Debugger::begin_sample() {
    if (!impl_ || !impl_->engine) return false;
    fv1_snapshot current{};
    fv1_get_snapshot(impl_->engine, &current);
    if (current.debug_sample_active != 0) return true;
    return fv1_debug_begin_sample(impl_->engine, impl_->input_l, impl_->input_r) == FV1_OK;
}

bool Debugger::step_instruction(DebugStep& step) {
    step = {};
    if (!begin_sample()) return false;
    if (fv1_debug_step_instruction(impl_->engine, &step.trace) != FV1_OK) return false;
    fv1_get_snapshot(impl_->engine, &step.snapshot);
    step.sample_finished = step.trace.sample_finished != 0;
    if (step.sample_finished) {
        if (fv1_debug_finish_sample(impl_->engine, &impl_->output_l, &impl_->output_r) != FV1_OK) return false;
        ++impl_->sample_index;
        fv1_get_snapshot(impl_->engine, &step.snapshot);
    }
    step.sample_index = impl_->sample_index;
    impl_->last_step = step;
    return true;
}

bool Debugger::step_sample(DebugStep& final_step, std::size_t max_instructions) {
    final_step = {};
    if (!begin_sample()) return false;
    for (std::size_t i = 0; i < max_instructions; ++i) {
        if (!step_instruction(final_step)) return false;
        if (final_step.sample_finished) return true;
    }
    return false;
}

bool Debugger::continue_until_breakpoint(const std::unordered_set<std::uint32_t>& pcs,
                                         DebugStep& final_step,
                                         std::size_t max_instructions) {
    final_step = {};
    if (!begin_sample()) return false;

    for (std::size_t i = 0; i < max_instructions; ++i) {
        fv1_snapshot before{};
        fv1_get_snapshot(impl_->engine, &before);
        if (i > 0 && pcs.contains(before.program_counter)) {
            final_step.snapshot = before;
            final_step.breakpoint_hit = true;
            final_step.sample_index = impl_->sample_index;
            impl_->last_step = final_step;
            return true;
        }
        if (!step_instruction(final_step)) return false;
        if (final_step.sample_finished) return true;
        if (pcs.contains(final_step.snapshot.program_counter)) {
            final_step.breakpoint_hit = true;
            return true;
        }
    }
    return false;
}

fv1_snapshot Debugger::snapshot() const noexcept {
    fv1_snapshot s{};
    if (impl_ && impl_->engine) fv1_get_snapshot(impl_->engine, &s);
    return s;
}

DebugStep Debugger::last_step() const noexcept {
    return impl_ ? impl_->last_step : DebugStep{};
}

bool Debugger::read_delay_word(std::uint32_t address, std::int32_t& value) const noexcept {
    return impl_ && impl_->engine &&
           fv1_read_delay_word(impl_->engine, address, &value) == FV1_OK;
}

bool Debugger::sample_active() const noexcept { return snapshot().debug_sample_active != 0; }
std::uint64_t Debugger::sample_index() const noexcept { return impl_ ? impl_->sample_index : 0u; }
float Debugger::last_output_left() const noexcept { return impl_ ? impl_->output_l : 0.0f; }
float Debugger::last_output_right() const noexcept { return impl_ ? impl_->output_r : 0.0f; }
fv1_engine* Debugger::engine() noexcept { return impl_ ? impl_->engine : nullptr; }
const fv1_engine* Debugger::engine() const noexcept { return impl_ ? impl_->engine : nullptr; }

} // namespace fv1
