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
    std::cout << "audio-host realtime DSP bypass API: PASS\n";
    return 0;
}
