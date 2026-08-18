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

    std::cout << "audio-host realtime DSP bypass/backend API: PASS\n";
    return 0;
}
