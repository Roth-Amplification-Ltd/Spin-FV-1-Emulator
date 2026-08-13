#pragma once

#include "fv1.h"
#include <stdexcept>
#include <utility>

namespace fv1 {

class Engine {
public:
    explicit Engine(fv1_config config = {32768.0, FV1_DELAY_REFERENCE_16})
        : handle_(fv1_create(&config)) {
        if (!handle_) throw std::runtime_error("fv1_create failed");
    }

    ~Engine() { fv1_destroy(handle_); }
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    Engine(Engine&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    Engine& operator=(Engine&& other) noexcept {
        if (this != &other) {
            fv1_destroy(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    fv1_engine* get() noexcept { return handle_; }
    const fv1_engine* get() const noexcept { return handle_; }

private:
    fv1_engine* handle_{};
};

} // namespace fv1
