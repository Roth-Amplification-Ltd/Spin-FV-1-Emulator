#pragma once

#include <fv1/sdk.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fv1::windows_frontend {

struct WasapiStreamStatus {
    bool running{};
    bool recovering{};
    std::wstring capture_name;
    std::wstring render_name;
    std::wstring last_error;
    std::uint64_t capture_frames{};
    std::uint64_t render_frames{};
    std::uint64_t output_underruns{};
    std::uint64_t output_overruns{};
    std::uint64_t recoveries{};
};

class WasapiAudioEngine final {
public:
    WasapiAudioEngine();
    ~WasapiAudioEngine();

    WasapiAudioEngine(const WasapiAudioEngine&) = delete;
    WasapiAudioEngine& operator=(const WasapiAudioEngine&) = delete;

    bool start(const std::array<std::uint8_t, FV1_SDK_PROGRAM_BYTES>& program,
               const std::array<float, 3>& pots);
    void stop();
    void set_pots(const std::array<float, 3>& pots) noexcept;

    [[nodiscard]] bool running() const noexcept { return running_.load(std::memory_order_acquire); }
    [[nodiscard]] bool active() const noexcept {
        return running_.load(std::memory_order_acquire) || recovering_.load(std::memory_order_acquire);
    }

    [[nodiscard]] WasapiStreamStatus status() const;
    void copy_scope(std::vector<float>& output, std::size_t maximum_samples = 1024u) const;

private:
    void worker_main();
    void set_error(std::wstring message);
    void set_endpoint_names(std::wstring capture, std::wstring render);

    std::array<std::uint8_t, FV1_SDK_PROGRAM_BYTES> program_{};
    std::array<std::atomic<float>, 3> pots_{};
    std::atomic<std::uint64_t> pot_revision_{0u};

    std::atomic<bool> running_{false};
    std::atomic<bool> recovering_{false};
    std::atomic<bool> stop_requested_{false};
    void* stop_event_{};
    std::thread worker_;

    std::atomic<std::uint64_t> capture_frames_{0u};
    std::atomic<std::uint64_t> render_frames_{0u};
    std::atomic<std::uint64_t> output_underruns_{0u};
    std::atomic<std::uint64_t> output_overruns_{0u};
    std::atomic<std::uint64_t> recoveries_{0u};

    static constexpr std::size_t kScopeCapacity = 4096u;
    std::array<std::atomic<float>, kScopeCapacity> scope_{};
    std::atomic<std::uint64_t> scope_write_index_{0u};

    mutable std::mutex info_mutex_;
    std::wstring capture_name_;
    std::wstring render_name_;
    std::wstring last_error_;
};

} // namespace fv1::windows_frontend
