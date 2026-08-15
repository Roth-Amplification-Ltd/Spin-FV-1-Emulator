#pragma once

#include <cstdint>
#include <string>

namespace fv1::windows_frontend {

struct WasapiEndpointInfo {
    bool available{};
    std::wstring name;
    std::uint32_t sample_rate{};
    std::uint16_t channels{};
    std::uint16_t bits_per_sample{};
};

struct WasapiProbeResult {
    WasapiEndpointInfo capture;
    WasapiEndpointInfo render;
    std::wstring diagnostic;
};

WasapiProbeResult probe_default_wasapi_endpoints();

} // namespace fv1::windows_frontend
