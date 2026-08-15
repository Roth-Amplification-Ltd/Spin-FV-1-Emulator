#include <fv1/analysis.hpp>
#include <fv1/audio_host.hpp>
#include <fv1/audio_source.hpp>
#include <fv1/runtime.hpp>
#include <fv1/spinasm.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

#ifndef FV1_PRODUCT_VERSION_STRING
#define FV1_PRODUCT_VERSION_STRING "1.0.0-rc1"
#endif



namespace {
struct Error : std::runtime_error { using std::runtime_error::runtime_error; };

struct Args {
    std::vector<std::string> v;
    bool has(const std::string& key) const { return std::find(v.begin(), v.end(), key) != v.end(); }
    std::string get(const std::string& key, const std::string& fallback = {}) const {
        for (std::size_t i = 0; i + 1 < v.size(); ++i) if (v[i] == key) return v[i + 1];
        return fallback;
    }
};


std::vector<std::uint8_t> read_file(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw Error("cannot open " + path.string());
    f.seekg(0, std::ios::end);
    const auto n = f.tellg();
    f.seekg(0, std::ios::beg);
    if (n < 0) throw Error("cannot size " + path.string());
    std::vector<std::uint8_t> data(static_cast<std::size_t>(n));
    if (!data.empty()) f.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!f && !data.empty()) throw Error("cannot read " + path.string());
    return data;
}

std::uint8_t hex_byte(const std::string& line, std::size_t pos, unsigned line_no) {
    if (pos + 2 > line.size()) throw Error("truncated Intel HEX line " + std::to_string(line_no));
    return static_cast<std::uint8_t>(std::stoul(line.substr(pos, 2), nullptr, 16));
}

std::vector<std::uint8_t> parse_hex(const fs::path& path) {
    std::ifstream f(path);
    if (!f) throw Error("cannot open " + path.string());
    std::map<std::uint32_t, std::uint8_t> mem;
    std::uint32_t upper = 0;
    std::string line;
    unsigned line_no = 0;
    while (std::getline(f, line)) {
        ++line_no;
        if (line.empty()) continue;
        if (line[0] != ':' || line.size() < 11) throw Error("invalid Intel HEX line " + std::to_string(line_no));
        const std::uint8_t len = hex_byte(line, 1, line_no);
        const std::uint16_t addr = static_cast<std::uint16_t>((hex_byte(line, 3, line_no) << 8) | hex_byte(line, 5, line_no));
        const std::uint8_t type = hex_byte(line, 7, line_no);
        std::uint8_t checksum = static_cast<std::uint8_t>(len + (addr >> 8) + (addr & 0xffu) + type);
        std::vector<std::uint8_t> payload;
        for (std::uint8_t i = 0; i < len; ++i) {
            const auto b = hex_byte(line, 9 + static_cast<std::size_t>(i) * 2, line_no);
            payload.push_back(b); checksum = static_cast<std::uint8_t>(checksum + b);
        }
        checksum = static_cast<std::uint8_t>(checksum + hex_byte(line, 9 + static_cast<std::size_t>(len) * 2, line_no));
        if (checksum != 0) throw Error("Intel HEX checksum failure at line " + std::to_string(line_no));
        if (type == 0x00) {
            const std::uint32_t base = upper + addr;
            for (std::size_t i = 0; i < payload.size(); ++i) mem[base + static_cast<std::uint32_t>(i)] = payload[i];
        } else if (type == 0x01) break;
        else if (type == 0x04 && payload.size() == 2)
            upper = (static_cast<std::uint32_t>(payload[0]) << 24) | (static_cast<std::uint32_t>(payload[1]) << 16);
    }
    if (mem.empty()) throw Error("Intel HEX contains no data");
    std::vector<std::uint8_t> data(mem.rbegin()->first + 1u, 0xffu);
    for (const auto& [address, byte] : mem) data[address] = byte;
    return data;
}

std::vector<std::uint8_t> load_program(const fs::path& source, unsigned slot) {
    std::string ext = source.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    std::vector<std::uint8_t> data;
    if (ext == ".spn") {
        const auto source_bytes = read_file(source);
        try {
            const auto compiled = fv1::spinasm::compile(std::string_view(
                reinterpret_cast<const char*>(source_bytes.data()), source_bytes.size()));
            data.assign(compiled.image.begin(), compiled.image.end());
        } catch (const fv1::spinasm::CompileError& error) {
            throw Error(source.string() + ": " + error.what());
        }
    } else {
        data = ext == ".hex" ? parse_hex(source) : read_file(source);
    }
    if (data.size() == FV1_PROGRAM_BYTES) return data;
    if (data.size() >= 8u * FV1_PROGRAM_BYTES) {
        if (slot > 7) throw Error("slot must be 0..7");
        const std::size_t begin = static_cast<std::size_t>(slot) * FV1_PROGRAM_BYTES;
        return {data.begin() + static_cast<std::ptrdiff_t>(begin),
                data.begin() + static_cast<std::ptrdiff_t>(begin + FV1_PROGRAM_BYTES)};
    }
    throw Error("program image must be 512 bytes or an eight-program bank");
}

void usage() {
    std::cout << R"(Spin FV-1 Emulator - Phase 2 Linux realtime host

Usage:
  fv1-live devices
  fv1-live run <program.spn|bin|hex> [source options] [audio options]

Sources (choose one; default is --live):
  --live                      Capture from the selected audio input.
  --file PATH                 Play a WAV file as the input stimulus.
  --no-loop                   Do not loop file input (looping is default).
  --loop-start SECONDS        File-loop start point.
  --loop-end SECONDS          File-loop end point.
  --sine HZ                   Sine generator.
  --sweep START:END:SECONDS   Logarithmic sine sweep.
  --white-noise               Deterministic white noise.
  --pink-noise                Deterministic pink-noise approximation.
  --impulse SECONDS           Impulse repeated at this period.

Audio/runtime:
  --input-device N            Capture index from `fv1-live devices`.
  --output-device N           Playback index; default OS device.
  --host-rate HZ              Audio interface rate (default 48000).
  --buffer FRAMES             Requested period size (default 256).
  --clock HZ                  Virtual FV-1 sample rate (default 32768).
  --resampler-quality N       SpeexDSP quality 0..10 (default 7).
  --pot0 X --pot1 X --pot2 X FV-1 controls, 0..1 (default 0.5).
  --slot N                    Program-bank slot 0..7.
  --seconds N                 Process exactly N seconds of host frames; otherwise Enter stops.
  --meter                     Print analyzer/runtime status while timed run executes.
  --full-delay-24             Diagnostic full-24-bit delay RAM model.
)";
}

std::unique_ptr<fv1::AudioSource> make_source(const Args& args, bool& needs_capture) {
    needs_capture = false;
    if (!args.get("--file").empty()) {
        auto src = std::make_unique<fv1::FileLoopSource>();
        std::string error;
        if (!src->load(args.get("--file"), &error)) throw Error(error);
        src->set_looping(!args.has("--no-loop"));
        const double begin_s = std::stod(args.get("--loop-start", "0"));
        const double end_s = std::stod(args.get("--loop-end", "0"));
        const auto rate = src->file_sample_rate();
        const std::uint64_t begin = static_cast<std::uint64_t>(std::max(0.0, begin_s) * rate);
        const std::uint64_t end = end_s > 0.0 ? static_cast<std::uint64_t>(end_s * rate) : 0;
        if (!src->set_loop_region(begin, end)) throw Error("invalid --loop-start/--loop-end region");
        src->play();
        return src;
    }

    fv1::TestSignalConfig tc;
    bool test = false;
    if (!args.get("--sine").empty()) {
        test = true; tc.kind = fv1::TestSignalKind::Sine; tc.frequency_hz = std::stod(args.get("--sine"));
    } else if (!args.get("--sweep").empty()) {
        test = true; tc.kind = fv1::TestSignalKind::Sweep;
        std::string s = args.get("--sweep");
        std::replace(s.begin(), s.end(), ':', ' ');
        std::istringstream is(s);
        if (!(is >> tc.frequency_hz >> tc.sweep_end_hz >> tc.sweep_seconds))
            throw Error("--sweep expects START:END:SECONDS");
    } else if (args.has("--white-noise")) {
        test = true; tc.kind = fv1::TestSignalKind::WhiteNoise;
    } else if (args.has("--pink-noise")) {
        test = true; tc.kind = fv1::TestSignalKind::PinkNoise;
    } else if (!args.get("--impulse").empty()) {
        test = true; tc.kind = fv1::TestSignalKind::Impulse;
        tc.impulse_period_seconds = std::stod(args.get("--impulse"));
    }
    if (test) return std::make_unique<fv1::TestSignalSource>(tc);

    needs_capture = true;
    return std::make_unique<fv1::LiveInputSource>();
}

void print_meter(const fv1::AudioHost& host, const fv1::Runtime& runtime,
                 const fv1::AnalyzerWorker& analyzer) {
    const auto h = host.stats();
    const auto r = runtime.stats();
    const auto a = analyzer.latest();
    std::cout << std::fixed << std::setprecision(3)
              << "cpu=" << (h.callback_cpu_load * 100.0) << "% "
              << "rms=" << a.rms_left << "/" << a.rms_right << " "
              << "peak=" << a.peak_left << "/" << a.peak_right << " "
              << "corr=" << a.correlation << " "
              << "dominant=" << std::setprecision(1) << a.dominant_frequency_hz << "Hz "
              << "underrun=" << r.output_underrun_frames << " "
              << "analysis-drop=" << analyzer.dropped_frames() << "\n";
}

int cmd_devices() {
    std::string error;
    const auto devices = fv1::AudioHost::enumerate(&error);
    if (!error.empty() && devices.empty()) throw Error(error);
    std::cout << "Playback devices:\n";
    for (const auto& d : devices) if (d.direction == fv1::AudioDeviceDirection::Playback)
        std::cout << "  [" << d.index << "] " << d.name << (d.is_default ? "  (default)" : "") << "\n";
    std::cout << "Capture devices:\n";
    for (const auto& d : devices) if (d.direction == fv1::AudioDeviceDirection::Capture)
        std::cout << "  [" << d.index << "] " << d.name << (d.is_default ? "  (default)" : "") << "\n";
    return 0;
}

int cmd_run(const Args& args) {
    if (args.v.size() < 2) throw Error("run requires a program path");
    const auto program = load_program(args.v[1], static_cast<unsigned>(std::stoul(args.get("--slot", "0"))));
    const std::uint32_t host_rate = static_cast<std::uint32_t>(std::stoul(args.get("--host-rate", "48000")));
    const std::uint32_t buffer = static_cast<std::uint32_t>(std::stoul(args.get("--buffer", "256")));
    const double fv1_rate = std::stod(args.get("--clock", "32768"));

    bool needs_capture = false;
    auto source = make_source(args, needs_capture);

    fv1::Runtime runtime;
    fv1::RuntimeConfig runtime_cfg;
    runtime_cfg.host_sample_rate = host_rate;
    runtime_cfg.fv1_sample_rate = fv1_rate;
    runtime_cfg.max_host_block_frames = std::max<std::size_t>(4096u, static_cast<std::size_t>(buffer) * 4u);
    runtime_cfg.resampler_quality = std::stoi(args.get("--resampler-quality", "7"));
    runtime_cfg.delay_model = args.has("--full-delay-24") ? FV1_DELAY_FULL_24 : FV1_DELAY_REFERENCE_16;
    if (!runtime.prepare(runtime_cfg)) throw Error("FV-1 runtime prepare failed");
    if (!runtime.load_program_bytes(program.data(), program.size())) throw Error("FV-1 program load failed");
    runtime.set_pots(std::stof(args.get("--pot0", "0.5")),
                     std::stof(args.get("--pot1", "0.5")),
                     std::stof(args.get("--pot2", "0.5")));

    fv1::AnalyzerWorker analyzer;
    if (!analyzer.prepare(host_rate, 1024, 32768)) throw Error("analyzer prepare failed");
    analyzer.start();

    fv1::AudioHost host;
    fv1::AudioHostConfig host_cfg;
    host_cfg.host_sample_rate = host_rate;
    host_cfg.period_frames = buffer;
    host_cfg.needs_capture = needs_capture;
    host_cfg.playback_device = std::stoi(args.get("--output-device", "-1"));
    host_cfg.capture_device = std::stoi(args.get("--input-device", "-1"));
    const double seconds = std::stod(args.get("--seconds", "0"));
    if (seconds > 0.0)
        host_cfg.stop_after_frames = static_cast<std::uint64_t>(std::llround(seconds * static_cast<double>(host_rate)));
    std::string error;
    if (!host.open(host_cfg, *source, runtime, &analyzer, &error)) throw Error(error);
    if (!host.start(&error)) throw Error(error);

    std::cout << "FV-1 realtime session started\n"
              << "  source:       " << source->name() << "\n"
              << "  host rate:    " << host_rate << " Hz\n"
              << "  virtual FV-1: " << fv1_rate << " Hz\n"
              << "  buffer:       " << buffer << " frames\n"
              << "  SRC:          " << (std::abs(static_cast<double>(host_rate) - fv1_rate) < 0.01
                                            ? "bypass (same rate)"
                                            : (runtime.using_speexdsp() ? "SpeexDSP" : "built-in linear fallback")) << "\n";

    if (seconds > 0.0) {
        auto next_meter = std::chrono::steady_clock::now();
        while (!host.is_finished()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (args.has("--meter") && std::chrono::steady_clock::now() >= next_meter) {
                print_meter(host, runtime, analyzer);
                next_meter = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            }
        }
    } else {
        std::cout << "Press Enter to stop..." << std::flush;
        std::cin.get();
    }

    host.stop();
    analyzer.stop();
    const auto hs = host.stats();
    const auto rs = runtime.stats();
    const auto as = analyzer.latest();
    std::cout << "\nSession complete\n"
              << "  callbacks:           " << hs.callbacks << "\n"
              << "  callback CPU load:   " << std::fixed << std::setprecision(2) << hs.callback_cpu_load * 100.0 << "%\n"
              << "  host frames:         " << rs.host_output_frames << "\n"
              << "  virtual FV-1 frames: " << rs.fv1_frames << "\n"
              << "  output underruns:    " << rs.output_underrun_frames << " frames\n"
              << "  analyzer drops:      " << analyzer.dropped_frames() << " frames\n"
              << "  final RMS:           " << as.rms_left << " / " << as.rms_right << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) { usage(); return 0; }
        Args args;
        for (int i = 1; i < argc; ++i) args.v.emplace_back(argv[i]);
        if (args.v[0] == "devices") return cmd_devices();
        if (args.v[0] == "run") return cmd_run(args);
        if (args.v[0] == "version" || args.v[0] == "--version") {
            std::cout << "Spin FV-1 Emulator " << FV1_PRODUCT_VERSION_STRING << "\n";
            return 0;
        }
        if (args.v[0] == "help" || args.v[0] == "--help" || args.v[0] == "-h") { usage(); return 0; }
        throw Error("unknown command: " + args.v[0]);
    } catch (const std::exception& e) {
        std::cerr << "fv1-live: error: " << e.what() << "\n";
        return 2;
    }
}
