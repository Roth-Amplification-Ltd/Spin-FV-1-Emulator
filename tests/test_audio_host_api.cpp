#include <fv1/audio_host.hpp>

#include <cassert>
#include <iostream>

int main() {
    fv1::AudioHost host;
    assert(host.dsp_enabled());
    host.set_dsp_enabled(false);
    assert(!host.dsp_enabled());
    host.set_dsp_enabled(true);
    assert(host.dsp_enabled());

    const std::string backend = fv1::AudioHost::backend_name();
    assert(!backend.empty());
#if defined(_WIN32)
    assert(backend.find("WASAPI") != std::string::npos);
#endif

    fv1::AudioHostConfig config;
    config.playback_device_id = "test-playback-id";
    config.capture_device_id = "test-capture-id";
    assert(config.playback_device_id == "test-playback-id");
    assert(config.capture_device_id == "test-capture-id");

    const auto stats = host.stats();
    assert(stats.callbacks == 0);
    assert(stats.min_callback_frames == 0);
    assert(stats.max_callback_frames == 0);
    assert(!stats.unexpected_device_stop);

    fv1::AudioDeviceInfo info;
    info.persistent_id = "test-endpoint";
    info.native_sample_rates = {44100, 48000};
    info.max_channels = 2;
    assert(info.native_sample_rates.size() == 2);
    assert(info.max_channels == 2);

    std::cout << "audio-host realtime DSP bypass/backend/device-health API: PASS\n";
    return 0;
}
