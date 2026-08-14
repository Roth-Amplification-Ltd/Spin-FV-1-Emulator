#include <fv1/analysis.hpp>
#include <fv1/audio_source.hpp>
#include <fv1/runtime.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {
int failures = 0;
void check(bool condition, const char* text) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << text << "\n"; }
}

std::array<std::uint8_t, FV1_PROGRAM_BYTES> passthrough_program() {
    std::array<std::uint8_t, FV1_PROGRAM_BYTES> p{};
    const std::array<std::uint8_t, 4> nop{0x00, 0x00, 0x00, 0x11};
    for (std::size_t i = 0; i < FV1_PROGRAM_WORDS; ++i)
        std::copy(nop.begin(), nop.end(), p.begin() + static_cast<std::ptrdiff_t>(i * 4));
    const std::array<std::uint8_t, 16> code{
        0x40,0x00,0x02,0x84, // RDAX ADCL,1
        0x00,0x00,0x02,0xc6, // WRAX DACL,0
        0x40,0x00,0x02,0xa4, // RDAX ADCR,1
        0x00,0x00,0x02,0xe6  // WRAX DACR,0
    };
    std::copy(code.begin(), code.end(), p.begin());
    return p;
}

void wr16(std::ostream& o, std::uint16_t v) {
    const char b[2]{static_cast<char>(v), static_cast<char>(v >> 8)}; o.write(b,2);
}
void wr32(std::ostream& o, std::uint32_t v) {
    const char b[4]{static_cast<char>(v), static_cast<char>(v >> 8), static_cast<char>(v >> 16), static_cast<char>(v >> 24)}; o.write(b,4);
}
void make_mono_wav(const fs::path& path, std::uint32_t rate) {
    constexpr std::size_t frames = 1000;
    const std::uint32_t data_size = static_cast<std::uint32_t>(frames * 2);
    std::ofstream o(path, std::ios::binary);
    o.write("RIFF",4); wr32(o,36+data_size); o.write("WAVE",4);
    o.write("fmt ",4); wr32(o,16); wr16(o,1); wr16(o,1); wr32(o,rate);
    wr32(o, rate*2); wr16(o,2); wr16(o,16);
    o.write("data",4); wr32(o,data_size);
    for (std::size_t i=0;i<frames;++i) {
        const double phase = 2.0 * 3.14159265358979323846 * 220.0 * static_cast<double>(i) / rate;
        const auto s = static_cast<std::int16_t>(std::sin(phase) * 12000.0);
        wr16(o, static_cast<std::uint16_t>(s));
    }
}

void make_extensible_pcm16_wav(const fs::path& path, std::uint32_t rate) {
    constexpr std::size_t frames = 128;
    constexpr std::uint16_t channels = 2;
    constexpr std::uint16_t bits = 16;
    constexpr std::uint32_t fmt_size = 40;
    const std::uint32_t data_size = static_cast<std::uint32_t>(frames * channels * (bits / 8));
    std::ofstream o(path, std::ios::binary);
    o.write("RIFF",4); wr32(o, 4 + 8 + fmt_size + 8 + data_size); o.write("WAVE",4);
    o.write("fmt ",4); wr32(o,fmt_size);
    wr16(o,0xfffe);                 // WAVE_FORMAT_EXTENSIBLE
    wr16(o,channels);
    wr32(o,rate);
    wr32(o,rate * channels * 2u);
    wr16(o,channels * 2u);
    wr16(o,bits);
    wr16(o,22);                     // cbSize
    wr16(o,bits);                   // valid bits
    wr32(o,0x00000003u);            // stereo FL|FR channel mask
    // KSDATAFORMAT_SUBTYPE_PCM = 00000001-0000-0010-8000-00AA00389B71
    const std::uint8_t guid[16]{
        0x01,0x00,0x00,0x00, 0x00,0x00,0x10,0x00,
        0x80,0x00,0x00,0xaa, 0x00,0x38,0x9b,0x71
    };
    o.write(reinterpret_cast<const char*>(guid),16);
    o.write("data",4); wr32(o,data_size);
    for (std::size_t i=0;i<frames;++i) {
        const auto l = static_cast<std::int16_t>(1000 + static_cast<int>(i));
        const auto r = static_cast<std::int16_t>(-1000 - static_cast<int>(i));
        wr16(o, static_cast<std::uint16_t>(l));
        wr16(o, static_cast<std::uint16_t>(r));
    }
}

void test_runtime_clock_bridge() {
    fv1::Runtime rt;
    fv1::RuntimeConfig cfg;
    cfg.host_sample_rate = 48000;
    cfg.fv1_sample_rate = 32768;
    cfg.max_host_block_frames = 256;
    check(rt.prepare(cfg), "runtime prepare 48k -> 32768");
    const auto program = passthrough_program();
    check(rt.load_program_bytes(program.data(), program.size()), "runtime load passthrough");

    std::vector<fv1::StereoFrame> in(240), out(240);
    double energy = 0.0;
    for (int block = 0; block < 200; ++block) {
        for (std::size_t i=0;i<in.size();++i) {
            const double t = static_cast<double>(block*240 + static_cast<int>(i))/48000.0;
            const float s = static_cast<float>(0.2*std::sin(2.0*3.14159265358979323846*440.0*t));
            in[i] = {s,s};
        }
        check(rt.process_block(in.data(), out.data(), out.size()), "runtime process block");
        for (const auto& f: out) {
            check(std::isfinite(f.left) && std::isfinite(f.right), "runtime output finite");
            energy += std::abs(f.left) + std::abs(f.right);
        }
    }
    const auto stats = rt.stats();
    check(stats.host_input_frames == 48000, "runtime host frame accounting");
    check(stats.fv1_frames > 32000 && stats.fv1_frames < 33550, "virtual clock generates ~32768 FV-1 frames per host second");
    check(energy > 10.0, "clock-bridged passthrough produces nonzero audio");
}

void test_fractional_virtual_clock() {
    fv1::Runtime rt;
    fv1::RuntimeConfig cfg;
    cfg.host_sample_rate = 48000.0;
    cfg.fv1_sample_rate = 46608.4;
    cfg.max_host_block_frames = 240;
    check(rt.prepare(cfg), "runtime prepare 48k -> 46.6084k fractional clock");
    const auto program = passthrough_program();
    check(rt.load_program_bytes(program.data(), program.size()), "fractional-clock passthrough load");
    std::vector<fv1::StereoFrame> in(240), out(240);
    for (int block = 0; block < 1000; ++block)
        check(rt.process_block(in.data(), out.data(), out.size()), "fractional-clock process block");
    const auto frames = rt.stats().fv1_frames;
    const std::uint64_t expected = 233042; // 5 seconds * 46608.4 Hz
    check(frames >= expected - 2 && frames <= expected + 2,
          "fractional virtual clock preserves 46.6084 kHz timing");
}

void test_extensible_wav_source() {
    const fs::path path = fs::temp_directory_path()/"fv1-phase2-extensible.wav";
    make_extensible_pcm16_wav(path,48000);
    fv1::FileLoopSource source;
    std::string error;
    check(source.load(path,&error), "WAVE_FORMAT_EXTENSIBLE PCM16 load");
    check(source.file_sample_rate()==48000, "extensible WAV sample rate");
    check(source.total_frames()==128, "extensible WAV frame count");
    std::error_code ec; fs::remove(path,ec);
}

void test_file_loop_source() {
    const fs::path path = fs::temp_directory_path()/"fv1-phase2-loop.wav";
    make_mono_wav(path,22050);
    fv1::FileLoopSource source;
    std::string error;
    check(source.load(path,&error), "file-loop WAV load");
    check(source.file_sample_rate()==22050, "file-loop source sample rate");
    check(source.prepare(48000,256), "file-loop prepare at different host rate");
    check(source.set_loop_region(100,900), "file-loop region");
    source.set_looping(true);
    source.play();
    std::vector<fv1::StereoFrame> out(4000);
    source.render(nullptr,out.data(),out.size());
    double energy=0.0;
    for (const auto& f:out) energy += std::abs(f.left)+std::abs(f.right);
    check(source.state()==fv1::TransportState::Playing, "loop source stays playing after wraps");
    check(energy>1.0, "loop source produces audio");
    std::error_code ec; fs::remove(path,ec);
}


void test_analyzer_silence_suppression() {
    fv1::AnalyzerWorker analyzer;
    check(analyzer.prepare(48000,1024,8192), "silent analyzer prepare");
    std::vector<fv1::StereoFrame> block(1024, fv1::StereoFrame{});
    analyzer.start();
    for (int i=0;i<4;++i) analyzer.push(block.data(),block.size());
    for (int i=0;i<100 && analyzer.latest().sequence==0;++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    analyzer.stop();
    const auto s=analyzer.latest();
    check(s.sequence>0,"silent analyzer produced snapshot");
    check(s.rms_left==0.0f && s.rms_right==0.0f,"silent analyzer RMS is zero");
    check(s.dominant_frequency_hz==0.0f,"silent analyzer suppresses meaningless dominant frequency");
}

void test_generator_and_analyzer() {
    fv1::TestSignalConfig cfg;
    cfg.kind=fv1::TestSignalKind::Sine;
    cfg.frequency_hz=1000.0;
    cfg.amplitude=0.5;
    fv1::TestSignalSource source(cfg);
    check(source.prepare(48000,1024), "sine source prepare");
    std::vector<fv1::StereoFrame> block(1024);

    fv1::AnalyzerWorker analyzer;
    check(analyzer.prepare(48000,1024,8192), "analyzer prepare");
    analyzer.start();
    for (int i=0;i<8;++i) {
        source.render(nullptr,block.data(),block.size());
        analyzer.push(block.data(),block.size());
    }
    for (int i=0;i<100 && analyzer.latest().sequence==0;++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    analyzer.stop();
    const auto s=analyzer.latest();
    check(s.sequence>0,"analyzer produced snapshot");
    check(s.rms_left>0.2f && s.rms_left<0.5f,"analyzer RMS sensible");
    check(s.peak_left>0.45f,"analyzer peak sensible");
    check(s.correlation>0.99f,"stereo correlation sensible");
    check(std::abs(s.dominant_frequency_hz-1000.0f)<60.0f,"FFT dominant frequency near 1kHz");
    check(!s.spectrum_db.empty(),"analyzer spectrum populated");
}
} // namespace

int main() {
    test_runtime_clock_bridge();
    test_fractional_virtual_clock();
    test_extensible_wav_source();
    test_file_loop_source();
    test_generator_and_analyzer();
    test_analyzer_silence_suppression();
    if (failures) {
        std::cerr << failures << " Phase-2 test(s) failed\n";
        return 1;
    }
    std::cout << "All Phase-2 runtime/source/analyzer tests passed.\n";
    return 0;
}
